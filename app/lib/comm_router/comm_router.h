#ifndef COMM_ROUTER_H
#define COMM_ROUTER_H

#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>

/**
 * @brief Interface transmit callback.
 *
 * The payload buffer is borrowed and valid only until the callback returns.
 * Callbacks run synchronously in thread context and are not ISR-safe.
 *
 * @param context Interface-specific callback context.
 * @param message_id Message identifier.
 * @param payload Payload bytes, or null when @p payload_length is zero.
 * @param payload_length Payload length in bytes.
 * @retval 0 Transmission accepted.
 * @retval Negative errno-style error on failure.
 */
typedef int (*comm_router_write_fn)(void *context, uint16_t message_id, const uint8_t *payload,
				    size_t payload_length);

/**
 * @brief Customer callback for an incoming message.
 *
 * The payload pointer is borrowed and valid only until the callback returns.
 * Callbacks run synchronously in thread context and are not ISR-safe.
 *
 * @param context Customer-specific callback context.
 * @param message_id Dispatched message identifier.
 * @param payload Message payload bytes.
 * @param payload_length Payload length in bytes.
 */
typedef void (*comm_router_receive_fn)(void *context, uint16_t message_id, const uint8_t *payload,
				       size_t payload_length);

/** @brief Compile-time communication interface descriptor. */
struct comm_router_interface {
	comm_router_write_fn write; /**< Interface transmit callback. */
	void *context; /**< Context passed to @p write. */
};

/** @brief Compile-time message ID to customer callback mapping. */
struct comm_router_customer {
	uint16_t message_id; /**< Unique message ID handled by this customer. */
	comm_router_receive_fn receive; /**< Callback invoked for matching messages. */
	void *context; /**< Context passed to @p receive. */
};

/**
 * @brief Declare a communication interface at file scope.
 *
 * @param name C identifier for the iterable descriptor.
 * @param write_callback Transmit callback.
 * @param callback_context Context passed to the callback.
 */
#define COMM_ROUTER_INTERFACE_DEFINE(name, write_callback, callback_context) \
	STRUCT_SECTION_ITERABLE(comm_router_interface, name) = { \
		.write = (write_callback), .context = (callback_context) \
	}

/**
 * @brief Declare a customer handler at file scope.
 *
 * @param name C identifier for the iterable descriptor.
 * @param message Unique message ID handled by this customer.
 * @param receive_callback Customer receive callback.
 * @param callback_context Context passed to the callback.
 */
#define COMM_ROUTER_CUSTOMER_DEFINE(name, message, receive_callback, callback_context) \
	STRUCT_SECTION_ITERABLE(comm_router_customer, name) = { \
		.message_id = (message), .receive = (receive_callback), .context = (callback_context) \
	}

/**
 * @brief Deliver one received logical message to the router.
 *
 * The payload pointer is borrowed for the duration of this synchronous call. This
 * function must not be called recursively from a customer callback and is not ISR-safe.
 *
 * @param interface Registered source descriptor.
 * @param message_id Message identifier.
 * @param payload Payload bytes, or null when @p payload_length is zero.
 * @param payload_length Payload length in bytes.
 * @retval 0 Input accepted.
 * @retval -EINVAL Payload pointer is null for nonzero length.
 * @retval -EINVAL Interface is null.
 * @retval -EMSGSIZE Payload exceeds the configured maximum.
 */
int comm_router_receive(struct comm_router_interface *interface, uint16_t message_id,
		const uint8_t *payload, size_t payload_length);

/**
 * @brief Broadcast one logical message through every declared interface.
 *
 * @param message_id Message identifier.
 * @param payload Payload bytes, or null when @p payload_length is zero.
 * @param payload_length Number of payload bytes.
 * @retval 0 Message was accepted by every interface callback.
 * @retval -EINVAL Payload pointer is null for nonzero length.
 * @retval -EMSGSIZE Payload exceeds the configured maximum.
 * @retval -ENOENT No interfaces are declared.
 * @retval Other The first interface callback error, after attempting every interface.
 */
int comm_router_send(uint16_t message_id, const uint8_t *payload, size_t payload_length);

#endif