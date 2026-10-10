#include <errno.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/sys/atomic.h>

#include "uart_transport.h"

#define RX_CHUNK_SIZE (32U)

struct rx_chunk {
	size_t length;
	uint8_t data[RX_CHUNK_SIZE];
};

struct state {
	const struct device *device;
	uart_transport_receive_fn receive;
	void *receive_context;
	uint8_t tx_buffer[CONFIG_COMM_ROUTER_MAX_PAYLOAD + BEAVER_PROTOCOL_FRAME_OVERHEAD];
	size_t tx_length;
	size_t tx_offset;
	int tx_result;
	atomic_t tx_failed;
	bool started;
};

static struct state generic_state;
static void rx_work_handler(struct k_work *work);

K_MSGQ_DEFINE(rx_queue, sizeof(struct rx_chunk), CONFIG_UART_TRANSPORT_GENERIC_RX_QUEUE_DEPTH, 4);
static K_MUTEX_DEFINE(tx_mutex);
static K_SEM_DEFINE(tx_done, 0, 1);
static K_WORK_DEFINE(rx_work, rx_work_handler);

static void rx_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	struct rx_chunk chunk;

	while (k_msgq_get(&rx_queue, &chunk, K_NO_WAIT) == 0) {
		(void)generic_state.receive(generic_state.receive_context, chunk.data, chunk.length);
	}
}

static void irq(const struct device *device, void *user_data)
{
	struct state *state = user_data;

	if (uart_irq_update(device) == 0) {
		return;
	}

	if (uart_irq_rx_ready(device)) {
		while (uart_irq_rx_ready(device)) {
			struct rx_chunk chunk;
			int received = uart_fifo_read(device, chunk.data, sizeof(chunk.data));

			if (received <= 0) {
				break;
			}
			chunk.length = (size_t)received;
			(void)k_msgq_put(&rx_queue, &chunk, K_NO_WAIT);
			(void)k_work_submit(&rx_work);
			if ((size_t)received < sizeof(chunk.data)) {
				break;
			}
		}
	}

	if (uart_irq_tx_ready(device)) {
		if (state->tx_offset < state->tx_length) {
			size_t remaining = state->tx_length - state->tx_offset;
			int written = uart_fifo_fill(device, &state->tx_buffer[state->tx_offset],
					     (int)remaining);

			if (written < 0) {
				state->tx_result = -EIO;
				atomic_set(&state->tx_failed, 1);
				uart_irq_tx_disable(device);
				k_sem_give(&tx_done);
				return;
			}
			state->tx_offset += (size_t)written;
		}

		if (state->tx_offset == state->tx_length && uart_irq_tx_complete(device)) {
			state->tx_result = 0;
			uart_irq_tx_disable(device);
			k_sem_give(&tx_done);
		}
	}
}

static int start(const struct device *device,
				       uart_transport_receive_fn receive,
				       void *receive_context)
{
	struct state *state = &generic_state;

	if (device == NULL || receive == NULL) {
		return -EINVAL;
	}
	if (state->started) {
		return -EALREADY;
	}
	if (!device_is_ready(device)) {
		return -ENODEV;
	}

	state->device = device;
	state->receive = receive;
	state->receive_context = receive_context;
	int result = uart_irq_callback_user_data_set(state->device, irq, state);
	if (result != 0) {
		return result;
	}

	state->started = true;
	uart_irq_rx_enable(state->device);
	return 0;
}


static int write(const struct device *device,
					const uint8_t *data, size_t length)
{
	struct state *state = &generic_state;

	if (device == NULL || (data == NULL && length != 0U)) {
		return -EINVAL;
	}
	if (!state->started || device != state->device) {
		return -ENOTCONN;
	}
	if (atomic_get(&state->tx_failed) != 0) {
		return -EIO;
	}
	if (length > sizeof(state->tx_buffer)) {
		return -EMSGSIZE;
	}
	if (length == 0U) {
		return 0;
	}

	k_mutex_lock(&tx_mutex, K_FOREVER);
	if (atomic_get(&state->tx_failed) != 0) {
		k_mutex_unlock(&tx_mutex);
		return -EIO;
	}
	memcpy(state->tx_buffer, data, length);
	state->tx_length = length;
	state->tx_offset = 0U;
	state->tx_result = -EINPROGRESS;
	k_sem_reset(&tx_done);
	uart_irq_tx_enable(state->device);
	int result = k_sem_take(&tx_done, K_MSEC(CONFIG_UART_TRANSPORT_GENERIC_TX_TIMEOUT_MS));
	if (result != 0) {
		uart_irq_tx_disable(state->device);
		atomic_set(&state->tx_failed, 1);
		result = -ETIMEDOUT;
	} else {
		result = state->tx_result;
	}
	k_mutex_unlock(&tx_mutex);
	return result;
}

const struct uart_transport_backend uart_transport_backend_api = {
	.start = start,
	.write = write,
};