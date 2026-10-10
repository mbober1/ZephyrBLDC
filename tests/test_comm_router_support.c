#include <errno.h>

#include <zephyr/ztest.h>

#include "commander.h"
#include "test_comm_router_support.h"

uint8_t transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD];
uint16_t transmitted_message_id;
size_t transmitted_length;
uint8_t selected_transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD];
uint16_t selected_transmitted_message_id;
size_t selected_transmitted_length;
unsigned int received_count;
uint16_t received_message_id;
uint8_t received_payload[16];
size_t received_payload_length;
bool fail_writes;
unsigned int failing_write_calls;
uint8_t uart_transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD +
			 BEAVER_PROTOCOL_FRAME_OVERHEAD];
size_t uart_transmitted_length;
uart_transport_receive_fn uart_receive;
void *uart_receive_context;
uint16_t commander_ack_message_id;
uint8_t commander_ack_status;
size_t commander_ack_length;
K_SEM_DEFINE(commander_ack_received, 0, 1);

static struct uart_transport *get_test_uart_transport(void);

static int test_write(void *context, uint16_t message_id, const uint8_t *payload,
		      size_t payload_length)
{
	ARG_UNUSED(context);

	if (payload_length > sizeof(transmitted)) {
		return -ENOMEM;
	}

	if (message_id == COMMANDER_ACK_MESSAGE_ID) {
		commander_ack_message_id = message_id;
		commander_ack_length = payload_length;
		if (payload_length == COMMANDER_ACK_PAYLOAD_SIZE) {
			commander_ack_status = payload[0];
		}
		k_sem_give(&commander_ack_received);
		return 0;
	}

	transmitted_message_id = message_id;
	transmitted_length = payload_length;
	if (payload_length != 0U) {
		memcpy(transmitted, payload, payload_length);
	}
	return 0;
}

static int selected_test_write(void *context, uint16_t message_id, const uint8_t *payload,
			       size_t payload_length)
{
	ARG_UNUSED(context);

	if (payload_length > sizeof(selected_transmitted)) {
		return -ENOMEM;
	}

	selected_transmitted_message_id = message_id;
	selected_transmitted_length = payload_length;
	if (payload_length != 0U) {
		memcpy(selected_transmitted, payload, payload_length);
	}
	return 0;
}

static int failing_test_write(void *context, uint16_t message_id, const uint8_t *payload,
			      size_t payload_length)
{
	ARG_UNUSED(context);
	ARG_UNUSED(message_id);
	ARG_UNUSED(payload);
	ARG_UNUSED(payload_length);

	if (!fail_writes) {
		return 0;
	}

	failing_write_calls++;
	return failing_write_calls == 1U ? -EIO : -ENOMEM;
}

static void test_receive(void *context, uint16_t message_id, const uint8_t *payload,
			 size_t payload_length)
{
	ARG_UNUSED(context);
	received_count++;
	received_message_id = message_id;
	received_payload_length = payload_length;
	if (payload_length <= sizeof(received_payload)) {
		memcpy(received_payload, payload, payload_length);
	}
}

static int fake_uart_start(const struct device *device,
			   uart_transport_receive_fn receive,
			   void *receive_context)
{
	ARG_UNUSED(device);
	uart_receive = receive;
	uart_receive_context = receive_context;
	return 0;
}

static int fake_uart_write(const struct device *device, const uint8_t *data,
			   size_t length)
{
	ARG_UNUSED(device);
	if (length > sizeof(uart_transmitted)) {
		return -ENOMEM;
	}

	memcpy(uart_transmitted, data, length);
	uart_transmitted_length = length;
	return 0;
}

static const struct uart_transport_backend fake_uart_backend = {
	.start = fake_uart_start,
	.write = fake_uart_write,
};

UART_TRANSPORT_DEFINE(test_uart_transport, &fake_uart_backend, NULL);

COMM_ROUTER_INTERFACE_DEFINE(test_interface, test_write, NULL);
COMM_ROUTER_INTERFACE_DEFINE(test_selected_interface, selected_test_write, NULL);
COMM_ROUTER_INTERFACE_DEFINE(test_failing_interface_one, failing_test_write, NULL);
COMM_ROUTER_INTERFACE_DEFINE(test_failing_interface_two, failing_test_write, NULL);
COMM_ROUTER_CUSTOMER_DEFINE(test_duplicate_customer, TEST_DUPLICATE_MESSAGE_ID, test_receive,
			    NULL);
COMM_ROUTER_CUSTOMER_DEFINE(test_round_trip_customer, TEST_ROUND_TRIP_MESSAGE_ID, test_receive,
			    NULL);
COMM_ROUTER_CUSTOMER_DEFINE(test_stream_customer, TEST_STREAM_MESSAGE_ID, test_receive, NULL);
COMM_ROUTER_CUSTOMER_DEFINE(test_invalid_frame_customer, TEST_INVALID_FRAME_MESSAGE_ID,
			    test_receive, NULL);

static struct uart_transport *get_test_uart_transport(void)
{
	return &test_uart_transport;
}

int start_test_uart_transport(void)
{
	return uart_transport_start(get_test_uart_transport());
}

void reset_test_state(void *fixture)
{
	ARG_UNUSED(fixture);
	transmitted_length = 0U;
	transmitted_message_id = 0U;
	selected_transmitted_length = 0U;
	selected_transmitted_message_id = 0U;
	received_count = 0U;
	received_message_id = 0U;
	received_payload_length = 0U;
	fail_writes = false;
	failing_write_calls = 0U;
	uart_transmitted_length = 0U;
	uart_receive = NULL;
	uart_receive_context = NULL;
	commander_ack_message_id = 0U;
	commander_ack_status = 0U;
	commander_ack_length = 0U;
	k_sem_reset(&commander_ack_received);
	get_test_uart_transport()->receive_length = 0U;
	get_test_uart_transport()->started = false;
}