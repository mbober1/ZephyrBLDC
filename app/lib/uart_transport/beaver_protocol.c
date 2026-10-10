#include <errno.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

#include "beaver_protocol.h"

enum frame_offset {
	START_MARKER_OFFSET = 0,
	VERSION_OFFSET,
	MESSAGE_ID_OFFSET,
	PAYLOAD_LENGTH_OFFSET = 4,
};

int beaver_protocol_frame_length(const uint8_t *data, size_t available, size_t *frame_length)
{
	int result = 0;

	if (data == NULL || frame_length == NULL) {
		result = -EINVAL;
	}
	else if (available < BEAVER_PROTOCOL_HEADER_SIZE) {
		result = -EAGAIN;
	}
	else if (data[START_MARKER_OFFSET] != BEAVER_PROTOCOL_START_MARKER ||
	    data[VERSION_OFFSET] != BEAVER_PROTOCOL_VERSION) {
		result = -EBADMSG;
	}
	else {
		size_t payload_length = sys_get_le16(&data[PAYLOAD_LENGTH_OFFSET]);
		if (payload_length > CONFIG_COMM_ROUTER_MAX_PAYLOAD) {
			result = -EMSGSIZE;
		}

		*frame_length = BEAVER_PROTOCOL_FRAME_OVERHEAD + payload_length;
	}
	return result;
}

int beaver_protocol_decode(const uint8_t *frame, size_t frame_length,
			   struct beaver_protocol_message *message)
{
	if (frame == NULL || message == NULL) {
		return -EINVAL;
	}

	size_t expected_length;
	int result = beaver_protocol_frame_length(frame, frame_length, &expected_length);
	if (result != 0) {
		return result;
	}
	if (frame_length < expected_length) {
		return -EAGAIN;
	}
	if (frame_length != expected_length) {
		return -EMSGSIZE;
	}

	size_t payload_length = sys_get_le16(&frame[PAYLOAD_LENGTH_OFFSET]);
	uint16_t received_crc = sys_get_le16(&frame[BEAVER_PROTOCOL_HEADER_SIZE + payload_length]);
	uint16_t expected_crc = crc16_itu_t(
		0xFFFFU, &frame[VERSION_OFFSET],
		BEAVER_PROTOCOL_HEADER_SIZE - VERSION_OFFSET + payload_length);
	if (received_crc != expected_crc) {
		return -EBADMSG;
	}

	message->message_id = sys_get_le16(&frame[MESSAGE_ID_OFFSET]);
	message->payload = &frame[BEAVER_PROTOCOL_HEADER_SIZE];
	message->payload_length = payload_length;
	return 0;
}

int beaver_protocol_encode(uint16_t message_id, const uint8_t *payload, size_t payload_length,
			   uint8_t *frame, size_t frame_capacity, size_t *frame_length)
{
	if ((payload == NULL && payload_length != 0U) || frame == NULL || frame_length == NULL) {
		return -EINVAL;
	}
	if (payload_length > CONFIG_COMM_ROUTER_MAX_PAYLOAD) {
		return -EMSGSIZE;
	}

	size_t encoded_length = BEAVER_PROTOCOL_FRAME_OVERHEAD + payload_length;
	if (frame_capacity < encoded_length) {
		return -ENOSPC;
	}

	frame[START_MARKER_OFFSET] = BEAVER_PROTOCOL_START_MARKER;
	frame[VERSION_OFFSET] = BEAVER_PROTOCOL_VERSION;
	sys_put_le16(message_id, &frame[MESSAGE_ID_OFFSET]);
	sys_put_le16((uint16_t)payload_length, &frame[PAYLOAD_LENGTH_OFFSET]);
	if (payload_length != 0U) {
		memcpy(&frame[BEAVER_PROTOCOL_HEADER_SIZE], payload, payload_length);
	}
	uint16_t crc = crc16_itu_t(0xFFFFU, &frame[VERSION_OFFSET], BEAVER_PROTOCOL_HEADER_SIZE - VERSION_OFFSET + payload_length);
	sys_put_le16(crc, &frame[BEAVER_PROTOCOL_HEADER_SIZE + payload_length]);
	*frame_length = encoded_length;
	return 0;
}