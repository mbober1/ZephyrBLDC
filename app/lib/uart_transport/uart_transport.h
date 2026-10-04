#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>

#include "beaver_protocol.h"
#include "comm_router.h"

typedef int (*uart_transport_receive_fn)(void *context, const uint8_t *data, size_t length);

struct uart_transport_backend {
	int (*start)(const struct device *device,
		     uart_transport_receive_fn receive, void *receive_context);
	int (*write)(const struct device *device, const uint8_t *data, size_t length);
};

extern const struct uart_transport_backend uart_transport_backend_api;

struct uart_transport {
	struct comm_router_interface *interface;
	const struct uart_transport_backend *backend;
	const struct device *device;
	size_t receive_length;
	uint8_t receive_frame[CONFIG_COMM_ROUTER_MAX_PAYLOAD + BEAVER_PROTOCOL_FRAME_OVERHEAD];
	bool started;
};

int uart_transport_start(struct uart_transport *transport);
int uart_transport_receive(void *context, const uint8_t *data, size_t length);
int uart_transport_write(void *context, uint16_t message_id, const uint8_t *payload,
			 size_t payload_length);

#define UART_TRANSPORT_DEFINE(name, backend_api, uart_device) \
	static struct uart_transport name; \
	COMM_ROUTER_INTERFACE_DEFINE(name##_router_interface, uart_transport_write, &name); \
	static struct uart_transport name = { \
		.interface = &name##_router_interface, \
		.backend = (backend_api), \
		.device = (uart_device), \
	}

#endif