#include <errno.h>

#include <zephyr/ztest.h>

#include "comm_router.h"
#include "uart_transport.h"

#define TEST_DUPLICATE_MESSAGE_ID 0x1101U
#define TEST_ROUND_TRIP_MESSAGE_ID 0x1102U
#define TEST_STREAM_MESSAGE_ID 0x1103U
#define TEST_INVALID_FRAME_MESSAGE_ID 0x1104U

static uint8_t transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD];
static uint16_t transmitted_message_id;
static size_t transmitted_length;
static uint8_t selected_transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD];
static uint16_t selected_transmitted_message_id;
static size_t selected_transmitted_length;
static unsigned int received_count;
static uint16_t received_message_id;
static uint8_t received_payload[16];
static size_t received_payload_length;
static bool fail_writes;
static unsigned int failing_write_calls;
static uint8_t uart_transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD +
				BEAVER_PROTOCOL_FRAME_OVERHEAD];
static size_t uart_transmitted_length;
static uart_transport_receive_fn uart_receive;
static void *uart_receive_context;

static int test_write(void *context, uint16_t message_id, const uint8_t *payload,
		      size_t payload_length)
{
	ARG_UNUSED(context);

	if (payload_length > sizeof(transmitted)) {
		return -ENOMEM;
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

static void reset_test_state(void *fixture)
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
	test_uart_transport.receive_length = 0U;
	test_uart_transport.started = false;
}

ZTEST_SUITE(comm_router, NULL, NULL, reset_test_state, NULL, NULL);

ZTEST(comm_router, test_router_broadcasts_logical_messages)
{
	const uint8_t payload[] = {0x10U, 0x20U, 0x30U};

	zassert_equal(comm_router_send(TEST_ROUND_TRIP_MESSAGE_ID, payload, sizeof(payload)),
		      0);
	zassert_equal(transmitted_message_id, TEST_ROUND_TRIP_MESSAGE_ID);
	zassert_equal(transmitted_length, sizeof(payload));
	zassert_mem_equal(transmitted, payload, sizeof(payload));
	zassert_equal(selected_transmitted_message_id, TEST_ROUND_TRIP_MESSAGE_ID);
	zassert_equal(selected_transmitted_length, sizeof(payload));
	zassert_mem_equal(selected_transmitted, payload, sizeof(payload));
	zassert_equal(uart_transmitted[0], BEAVER_PROTOCOL_START_MARKER);
	zassert_equal(uart_transmitted_length,
		      sizeof(payload) + BEAVER_PROTOCOL_FRAME_OVERHEAD);
}

ZTEST(comm_router, test_router_returns_first_write_error_after_broadcast)
{
	const uint8_t payload[] = {0x10U};

	fail_writes = true;
	zassert_equal(comm_router_send(TEST_ROUND_TRIP_MESSAGE_ID, payload, sizeof(payload)), -EIO);
	zassert_equal(failing_write_calls, 2U);
}

ZTEST(comm_router, test_router_dispatches_logical_messages)
{
	const uint8_t payload[] = {0x44U, 0x55U};
	uint8_t oversized_payload[CONFIG_COMM_ROUTER_MAX_PAYLOAD + 1U];

	zassert_equal(comm_router_receive(&test_interface, TEST_STREAM_MESSAGE_ID, payload,
					  sizeof(payload)),
		      0);
	zassert_equal(received_count, 1U);
	zassert_equal(received_message_id, TEST_STREAM_MESSAGE_ID);
	zassert_equal(received_payload_length, sizeof(payload));
	zassert_mem_equal(received_payload, payload, sizeof(payload));
	zassert_equal(comm_router_receive(&test_interface, TEST_STREAM_MESSAGE_ID, NULL, 1U),
		      -EINVAL);
	zassert_equal(comm_router_receive(&test_interface, TEST_STREAM_MESSAGE_ID, oversized_payload,
					  sizeof(oversized_payload)),
		      -EMSGSIZE);
}

ZTEST(comm_router, test_uart_transport_encodes_and_decodes_messages)
{
	const uint8_t payload[] = {0x61U, 0x72U, 0x83U};

	zassert_equal(uart_transport_start(&test_uart_transport), 0);
	zassert_equal(uart_transport_start(&test_uart_transport), -EALREADY);
	zassert_equal(uart_transport_receive(NULL, payload, sizeof(payload)), -EINVAL);
	zassert_not_null(uart_receive);
	zassert_equal(comm_router_send(TEST_STREAM_MESSAGE_ID, payload, sizeof(payload)),
		       0);
	zassert_equal(uart_transmitted[0], BEAVER_PROTOCOL_START_MARKER);
	zassert_equal(uart_transmitted[1], BEAVER_PROTOCOL_VERSION);
	zassert_equal(uart_transmitted[2], (uint8_t)(TEST_STREAM_MESSAGE_ID & 0xFFU));
	zassert_equal(uart_transmitted[3], (uint8_t)(TEST_STREAM_MESSAGE_ID >> 8));
	zassert_equal(uart_transmitted[4], sizeof(payload));
	zassert_equal(uart_transmitted[5], 0U);
	zassert_mem_equal(&uart_transmitted[BEAVER_PROTOCOL_HEADER_SIZE], payload,
			  sizeof(payload));
	zassert_equal(uart_receive(uart_receive_context, uart_transmitted, uart_transmitted_length), 0);
	zassert_equal(received_count, 1U);
	zassert_equal(received_message_id, TEST_STREAM_MESSAGE_ID);
	zassert_mem_equal(received_payload, payload, sizeof(payload));
}

ZTEST(comm_router, test_uart_receive_partial_and_concatenated_frames)
{
	const uint8_t first_payload[] = {0x44U, 0x55U};
	const uint8_t second_payload[] = {0x66U};
	uint8_t first_frame[sizeof(first_payload) + BEAVER_PROTOCOL_FRAME_OVERHEAD];
	size_t first_frame_length;

	zassert_equal(uart_transport_start(&test_uart_transport), 0);
	zassert_equal(comm_router_send(TEST_STREAM_MESSAGE_ID, first_payload, sizeof(first_payload)),
		      0);
	memcpy(first_frame, uart_transmitted, uart_transmitted_length);
	first_frame_length = uart_transmitted_length;
	zassert_equal(comm_router_send(TEST_STREAM_MESSAGE_ID, second_payload, sizeof(second_payload)),
		      0);

	zassert_equal(uart_receive(uart_receive_context, first_frame, 3U), 0);
	zassert_equal(received_count, 0U);
	zassert_equal(uart_receive(uart_receive_context, &first_frame[3], first_frame_length - 3U),
		      0);
	zassert_equal(uart_receive(uart_receive_context, uart_transmitted, uart_transmitted_length),
		      0);
	zassert_equal(received_count, 2U);
	zassert_equal(received_payload_length, sizeof(second_payload));
	zassert_mem_equal(received_payload, second_payload, sizeof(second_payload));
}

ZTEST(comm_router, test_uart_receive_discards_bad_crc_and_version)
{
	const uint8_t payload[] = {0x01U};
	uint8_t stream[3U * (sizeof(payload) + BEAVER_PROTOCOL_FRAME_OVERHEAD)];
	size_t frame_length;

	zassert_equal(uart_transport_start(&test_uart_transport), 0);
	zassert_equal(comm_router_send(TEST_INVALID_FRAME_MESSAGE_ID, payload, sizeof(payload)), 0);
	frame_length = uart_transmitted_length;
	memcpy(stream, uart_transmitted, frame_length);
	stream[frame_length - 1U] ^= 0x01U;
	memcpy(&stream[frame_length], uart_transmitted, frame_length);
	stream[frame_length + 1U] = 2U;
	memcpy(&stream[2U * frame_length], uart_transmitted, frame_length);

	zassert_equal(uart_receive(uart_receive_context, stream, 3U * frame_length), 0);
	zassert_equal(received_count, 1U);
	zassert_equal(received_message_id, TEST_INVALID_FRAME_MESSAGE_ID);
	zassert_mem_equal(received_payload, payload, sizeof(payload));
}