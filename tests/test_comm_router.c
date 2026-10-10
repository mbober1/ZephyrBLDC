#include <errno.h>

#include <zephyr/ztest.h>

#include "test_comm_router_support.h"

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