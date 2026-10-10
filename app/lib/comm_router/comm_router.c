
#include "comm_router.h"

static K_MUTEX_DEFINE(receive_mutex);

int comm_router_receive(struct comm_router_interface *interface, uint16_t message_id,
		const uint8_t *payload, size_t payload_length)
{
	if (payload == NULL && payload_length != 0U) {
		return -EINVAL;
	}
	if (interface == NULL) {
		return -EINVAL;
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
	int result = 0;
	bool interface_found = false;

	if (payload == NULL && payload_length != 0U) {
		result = -EINVAL;
	}
	else if (payload_length > CONFIG_COMM_ROUTER_MAX_PAYLOAD) {
		result = -EMSGSIZE;
	}
	else
	{
		STRUCT_SECTION_FOREACH(comm_router_interface, interface) {
			interface_found = true;
			int error = 0;
			if (NULL == interface->write) {
				error = -EINVAL;
			} else {
				error = interface->write(interface->context, message_id, payload, payload_length);
			}

			if (result == 0 && error != 0) {
				result = error;
			}
		}

		if (false == interface_found) {
			result = -ENOENT;
		}
	}

	return result;
}