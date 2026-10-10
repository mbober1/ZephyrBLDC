#include <errno.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/sys/atomic.h>

#include "uart_transport.h"

#define RX_BUFFER_COUNT 2U

struct rx_chunk {
	size_t length;
	uint8_t data[CONFIG_UART_TRANSPORT_STM32_RX_BUFFER_SIZE];
};

struct uart_transport_stm32 {
	const struct device *device;
	uart_transport_receive_fn receive;
	void *receive_context;
	uint8_t rx_buffers[RX_BUFFER_COUNT][CONFIG_UART_TRANSPORT_STM32_RX_BUFFER_SIZE];
	bool rx_buffer_in_use[RX_BUFFER_COUNT];
	uint8_t tx_buffer[CONFIG_COMM_ROUTER_MAX_PAYLOAD + BEAVER_PROTOCOL_FRAME_OVERHEAD];
	int tx_result;
	atomic_t tx_active;
	bool started;
};

static struct uart_transport_stm32 stm32_state;
static void rx_work_handler(struct k_work *work);

static K_MSGQ_DEFINE(rx_queue, sizeof(struct rx_chunk), CONFIG_UART_TRANSPORT_STM32_RX_QUEUE_DEPTH, 4);
static K_MUTEX_DEFINE(tx_mutex);
static K_SEM_DEFINE(tx_done, 0, 1);
static K_WORK_DEFINE(rx_work, rx_work_handler);

static void queue_rx(struct uart_transport_stm32 *context,
					  uint8_t *buffer, size_t offset, size_t length)
{
	struct rx_chunk chunk;

	if (length == 0U || length > sizeof(chunk.data) ||
	    offset > CONFIG_UART_TRANSPORT_STM32_RX_BUFFER_SIZE - length) {
		return;
	}

	memcpy(chunk.data, &buffer[offset], length);
	chunk.length = length;
	if (k_msgq_put(&rx_queue, &chunk, K_NO_WAIT) == 0) {
		(void)k_work_submit(&rx_work);
	}
}

static void rx_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	struct rx_chunk chunk;

	while (k_msgq_get(&rx_queue, &chunk, K_NO_WAIT) == 0) {
		(void)stm32_state.receive(stm32_state.receive_context, chunk.data, chunk.length);
	}
}

static size_t find_free_buffer(struct uart_transport_stm32 *context)
{
	for (size_t index = 0U; index < RX_BUFFER_COUNT; index++) {
		if (!context->rx_buffer_in_use[index]) {
			return index;
		}
	}

	return RX_BUFFER_COUNT;
}

static void uart_callback(const struct device *device,
					      struct uart_event *event, void *user_data)
{
	struct uart_transport_stm32 *context = user_data;

	if (context == NULL || device != context->device) {
		return;
	}

	switch (event->type) {
	case UART_RX_RDY: {
		const struct uart_event_rx *rx = &event->data.rx;

		queue_rx(context, rx->buf, rx->offset, rx->len);
		break;
	}
	case UART_RX_STOPPED: {
		const struct uart_event_rx *rx = &event->data.rx_stop.data;

		queue_rx(context, rx->buf, rx->offset, rx->len);
		break;
	}
	case UART_RX_BUF_REQUEST: {
		size_t index = find_free_buffer(context);

		if (index < RX_BUFFER_COUNT &&
		    uart_rx_buf_rsp(device, context->rx_buffers[index],
				   sizeof(context->rx_buffers[index])) == 0) {
			context->rx_buffer_in_use[index] = true;
		}
		break;
	}
	case UART_RX_BUF_RELEASED:
		for (size_t index = 0U; index < RX_BUFFER_COUNT; index++) {
			if (event->data.rx_buf.buf == context->rx_buffers[index]) {
				context->rx_buffer_in_use[index] = false;
				break;
			}
		}
		break;
	case UART_TX_DONE:
		if (atomic_get(&context->tx_active) != 0) {
			atomic_clear(&context->tx_active);
			context->tx_result = 0;
			k_sem_give(&tx_done);
		}
		break;
	case UART_TX_ABORTED:
		if (atomic_get(&context->tx_active) != 0) {
			atomic_clear(&context->tx_active);
			if (context->tx_result != -ETIMEDOUT) {
				context->tx_result = -EIO;
			}
			k_sem_give(&tx_done);
		}
		break;
	default:
		break;
	}
}

static int start(const struct device *device,
				      uart_transport_receive_fn receive,
				      void *receive_context)
{
	struct uart_transport_stm32 *context = &stm32_state;
	int result;

	if (device == NULL || receive == NULL) {
		return -EINVAL;
	}
	if (context->started) {
		return -EALREADY;
	}
	if (!device_is_ready(device)) {
		return -ENODEV;
	}

	context->device = device;
	context->receive = receive;
	context->receive_context = receive_context;
	context->rx_buffer_in_use[0] = true;
	context->rx_buffer_in_use[1] = false;

	result = uart_callback_set(context->device, uart_callback, context);
	if (result != 0) {
		return result;
	}

	result = uart_rx_enable(context->device, context->rx_buffers[0],
				 sizeof(context->rx_buffers[0]),
				 CONFIG_UART_TRANSPORT_STM32_RX_TIMEOUT_US);
	if (result != 0) {
		return result;
	}

	context->started = true;
	return 0;
}


static int write(const struct device *device,
				      const uint8_t *data, size_t length)
{
	struct uart_transport_stm32 *context = &stm32_state;
	int result;

	if (device == NULL || (data == NULL && length != 0U)) {
		return -EINVAL;
	}
	if (!context->started || device != context->device) {
		return -ENOTCONN;
	}
	if (length == 0U) {
		return 0;
	}
	if (length > sizeof(context->tx_buffer)) {
		return -EMSGSIZE;
	}

	k_mutex_lock(&tx_mutex, K_FOREVER);
	if (atomic_get(&context->tx_active) != 0) {
		k_mutex_unlock(&tx_mutex);
		return -EBUSY;
	}
	memcpy(context->tx_buffer, data, length);
	atomic_set(&context->tx_active, 1);
	context->tx_result = -EINPROGRESS;
	k_sem_reset(&tx_done);
	result = uart_tx(context->device, context->tx_buffer, length, SYS_FOREVER_US);
	if (result == 0) {
		result = k_sem_take(&tx_done,
				    K_MSEC(CONFIG_UART_TRANSPORT_STM32_TX_TIMEOUT_MS));
		if (result != 0) {
			context->tx_result = -ETIMEDOUT;
			(void)uart_tx_abort(context->device);
			result = -ETIMEDOUT;
		} else {
			result = context->tx_result;
		}
	} else {
		atomic_clear(&context->tx_active);
	}
	k_mutex_unlock(&tx_mutex);
	return result;
}

const struct uart_transport_backend uart_transport_backend_api = {
	.start = start,
	.write = write,
};