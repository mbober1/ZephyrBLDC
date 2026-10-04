#ifndef BEAVER_PROTOCOL_H
#define BEAVER_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define BEAVER_PROTOCOL_START_MARKER (0xA5U)
#define BEAVER_PROTOCOL_VERSION (1U)
#define BEAVER_PROTOCOL_HEADER_SIZE (6U)
#define BEAVER_PROTOCOL_CRC_SIZE (2U)
#define BEAVER_PROTOCOL_FRAME_OVERHEAD \
	(BEAVER_PROTOCOL_HEADER_SIZE + BEAVER_PROTOCOL_CRC_SIZE)

struct beaver_protocol_message {
	uint16_t message_id;
	const uint8_t *payload;
	size_t payload_length;
};

/**
 * @brief Determine the expected size of a Beaver frame from its header.
 *
 * @param data Input beginning at the start marker.
 * @param available Number of bytes available in @p data.
 * @param frame_length Receives the total frame size on success.
 * @retval 0 Header is valid and @p frame_length was written.
 * @retval -EINVAL An argument is null.
 * @retval -EAGAIN The complete header is not available yet.
 * @retval -EBADMSG The marker or version is invalid.
 * @retval -EMSGSIZE The payload exceeds the configured maximum.
 */
int beaver_protocol_frame_length(const uint8_t *data, size_t available, size_t *frame_length);

/**
 * @brief Decode and validate exactly one complete Beaver frame.
 *
 * The decoded payload pointer refers to @p frame and remains valid only while
 * that input buffer remains valid.
 *
 * @param frame Complete encoded frame.
 * @param frame_length Number of bytes in @p frame.
 * @param message Receives the decoded message on success.
 * @retval 0 Frame decoded successfully.
 * @retval -EINVAL An argument is null.
 * @retval -EAGAIN The frame is incomplete.
 * @retval -EBADMSG The marker, version, or CRC is invalid.
 * @retval -EMSGSIZE The payload is too large or the input has trailing bytes.
 */
int beaver_protocol_decode(const uint8_t *frame, size_t frame_length,
			   struct beaver_protocol_message *message);

/**
 * @brief Encode a message into a Beaver frame.
 *
 * The encoded frame uses little-endian multi-byte fields and CRC-16/CCITT-FALSE.
 *
 * @param message_id Message identifier to encode.
 * @param payload Payload bytes, or null when @p payload_length is zero.
 * @param payload_length Number of payload bytes.
 * @param frame Caller-owned output buffer.
 * @param frame_capacity Capacity of @p frame in bytes.
 * @param frame_length Receives the encoded frame size on success.
 * @retval 0 Frame encoded successfully.
 * @retval -EINVAL An argument is invalid.
 * @retval -EMSGSIZE The payload exceeds the configured maximum.
 * @retval -ENOSPC The output buffer is too small.
 */
int beaver_protocol_encode(uint16_t message_id, const uint8_t *payload, size_t payload_length,
			   uint8_t *frame, size_t frame_capacity, size_t *frame_length);

#endif