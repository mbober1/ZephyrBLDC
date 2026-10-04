#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "uart_transport.h"

static K_MUTEX_DEFINE(receive_mutex);

static void discard_received_prefix(struct uart_transport *transport, size_t length)
{
	if (length >= transport->receive_length) {
		transport->receive_length = 0U;
		return;
	}

	transport->receive_length -= length;
	memmove(transport->receive_frame, &transport->receive_frame[length],
		transport->receive_length);
}

static int process_received_frames(struct uart_transport *transport)
{
	int first_error = 0;

	while (transport->receive_length > 0U) {
		uint8_t *frame = transport->receive_frame;
		uint8_t *start = memchr(frame, BEAVER_PROTOCOL_START_MARKER,
					transport->receive_length);

		if (start == NULL) {
			transport->receive_length = 0U;
			return first_error;
		}
		discard_received_prefix(transport, (size_t)(start - frame));
		frame = transport->receive_frame;

		size_t frame_length;
		int result = beaver_protocol_frame_length(frame, transport->receive_length,
							    &frame_length);
		if (result == -EAGAIN) {
			return first_error;
		}
		if (result != 0) {
			discard_received_prefix(transport, 1U);
			continue;
		}
		if (transport->receive_length < frame_length) {
			return first_error;
		}

		struct beaver_protocol_message message;
		if (beaver_protocol_decode(frame, frame_length, &message) == 0) {
			result = comm_router_receive(transport->interface, message.message_id,
					     message.payload, message.payload_length);
			if (first_error == 0 && result != 0) {
				first_error = result;
			}
		}

		discard_received_prefix(transport, frame_length);
	}

	return first_error;
}

int uart_transport_start(struct uart_transport *transport)
{
	if (transport == NULL || transport->backend == NULL || transport->backend->start == NULL ||
	    transport->backend->write == NULL) {
		return -EINVAL;
	}
  
	if (transport->started) {
		return -EALREADY;
	}

	int result = transport->backend->start(transport->device, uart_transport_receive, transport);

	if (result == 0) {
		transport->started = true;
	}

	return result;
}

int uart_transport_receive(void *context, const uint8_t *data, size_t length)
{
	struct uart_transport *transport = context;

	if (transport == NULL || (data == NULL && length != 0U)) {
		return -EINVAL;
	}

	k_mutex_lock(&receive_mutex, K_FOREVER);
	size_t offset = 0U;
	int first_error = 0;
	while (offset < length) {
		int result = process_received_frames(transport);
		if (first_error == 0 && result != 0) {
			first_error = result;
		}

		size_t buffer_space = sizeof(transport->receive_frame) - transport->receive_length;
		if (buffer_space == 0U) {
			k_mutex_unlock(&receive_mutex);
			return first_error != 0 ? first_error : -EMSGSIZE;
		}

		size_t copy_length = length - offset;
		if (copy_length > buffer_space) {
			copy_length = buffer_space;
		}
		memcpy(&transport->receive_frame[transport->receive_length], &data[offset], copy_length);
		transport->receive_length += copy_length;
		offset += copy_length;
	}

	int result = process_received_frames(transport);
	if (first_error == 0) {
		first_error = result;
	}
	k_mutex_unlock(&receive_mutex);
	return first_error;
}

int uart_transport_write(void *context, uint16_t message_id, const uint8_t *payload,
			 size_t payload_length)
{
	struct uart_transport *transport = context;

	if (transport == NULL || transport->backend == NULL || transport->backend->write == NULL) {
		return -EINVAL;
	}

	uint8_t frame[CONFIG_COMM_ROUTER_MAX_PAYLOAD + BEAVER_PROTOCOL_FRAME_OVERHEAD];
	size_t frame_length;
	int result = beaver_protocol_encode(message_id, payload, payload_length, frame, sizeof(frame),
					    &frame_length);
	if (result != 0) {
		return result;
	}

	return transport->backend->write(transport->device, frame, frame_length);
}