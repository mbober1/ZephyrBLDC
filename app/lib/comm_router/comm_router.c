#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "comm_router.h"

static K_MUTEX_DEFINE(receive_mutex);

static bool is_registered_interface(const struct comm_router_interface *candidate)
{
	STRUCT_SECTION_FOREACH(comm_router_interface, interface) {
		if (interface == candidate) {
			return true;
		}
	}

	return false;
}

int comm_router_receive(struct comm_router_interface *interface, uint16_t message_id,
		const uint8_t *payload, size_t payload_length)
{
	if (payload == NULL && payload_length != 0U) {
		return -EINVAL;
	}
	if (interface == NULL) {
		return -EINVAL;
	}
	if (!is_registered_interface(interface)) {
		return -ENOENT;
	}
	if (payload_length > CONFIG_COMM_ROUTER_MAX_PAYLOAD) {
		return -EMSGSIZE;
	}

	k_mutex_lock(&receive_mutex, K_FOREVER);
	STRUCT_SECTION_FOREACH(comm_router_customer, customer) {
		if (customer->message_id == message_id) {
			customer->receive(customer->context, message_id, payload, payload_length);
			break;
		}
	}
	k_mutex_unlock(&receive_mutex);

	return 0;
}

int comm_router_send(uint16_t message_id, const uint8_t *payload, size_t payload_length)
{
	if ((payload == NULL && payload_length != 0U) ||
	    payload_length > CONFIG_COMM_ROUTER_MAX_PAYLOAD) {
		return payload_length > CONFIG_COMM_ROUTER_MAX_PAYLOAD ? -EMSGSIZE : -EINVAL;
	}

	bool found_interface = false;
	int first_error = 0;
	STRUCT_SECTION_FOREACH(comm_router_interface, interface) {
		found_interface = true;
		int write_result = interface->write != NULL
				   ? interface->write(interface->context, message_id, payload, payload_length)
				   : -EINVAL;

		if (first_error == 0 && write_result != 0) {
			first_error = write_result;
		}
	}

	return found_interface ? first_error : -ENOENT;
}