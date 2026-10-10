#include <errno.h>

#include <zephyr/ztest.h>

#include "test_comm_router_support.h"

ZTEST_SUITE(uart_transport, NULL, NULL, reset_test_state, NULL, NULL);

ZTEST(uart_transport, test_uart_transport_encodes_and_decodes_messages)
{
	const uint8_t payload[] = {0x61U, 0x72U, 0x83U};

	zassert_equal(start_test_uart_transport(), 0);
	zassert_equal(start_test_uart_transport(), -EALREADY);
	zassert_equal(uart_transport_receive(NULL, payload, sizeof(payload)), -EINVAL);
	zassert_not_null(uart_receive);
	zassert_equal(comm_router_send(TEST_STREAM_MESSAGE_ID, payload, sizeof(payload)), 0);
	zassert_equal(uart_transmitted[0], BEAVER_PROTOCOL_START_MARKER);
	zassert_equal(uart_transmitted[1], BEAVER_PROTOCOL_VERSION);
	zassert_equal(uart_transmitted[2], (uint8_t)(TEST_STREAM_MESSAGE_ID & 0xFFU));
	zassert_equal(uart_transmitted[3], (uint8_t)(TEST_STREAM_MESSAGE_ID >> 8));
	zassert_equal(uart_transmitted[4], sizeof(payload));
	zassert_equal(uart_transmitted[5], 0U);
	zassert_mem_equal(&uart_transmitted[BEAVER_PROTOCOL_HEADER_SIZE], payload,
			  sizeof(payload));
	zassert_equal(uart_receive(uart_receive_context, uart_transmitted, uart_transmitted_length),
		      0);
	zassert_equal(received_count, 1U);
	zassert_equal(received_message_id, TEST_STREAM_MESSAGE_ID);
	zassert_mem_equal(received_payload, payload, sizeof(payload));
}

ZTEST(uart_transport, test_uart_receive_partial_and_concatenated_frames)
{
	const uint8_t first_payload[] = {0x44U, 0x55U};
	const uint8_t second_payload[] = {0x66U};
	uint8_t first_frame[sizeof(first_payload) + BEAVER_PROTOCOL_FRAME_OVERHEAD];
	size_t first_frame_length;

	zassert_equal(start_test_uart_transport(), 0);
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

ZTEST(uart_transport, test_uart_receive_discards_bad_crc_and_version)
{
	const uint8_t payload[] = {0x01U};
	uint8_t stream[3U * (sizeof(payload) + BEAVER_PROTOCOL_FRAME_OVERHEAD)];
	size_t frame_length;

	zassert_equal(start_test_uart_transport(), 0);
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