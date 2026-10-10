#ifndef TEST_COMM_ROUTER_SUPPORT_H
#define TEST_COMM_ROUTER_SUPPORT_H

#include <zephyr/kernel.h>

#include "comm_router.h"
#include "uart_transport.h"

#define TEST_DUPLICATE_MESSAGE_ID 0x1101U
#define TEST_ROUND_TRIP_MESSAGE_ID 0x1102U
#define TEST_STREAM_MESSAGE_ID 0x1103U
#define TEST_INVALID_FRAME_MESSAGE_ID 0x1104U

extern uint8_t transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD];
extern uint16_t transmitted_message_id;
extern size_t transmitted_length;
extern uint8_t selected_transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD];
extern uint16_t selected_transmitted_message_id;
extern size_t selected_transmitted_length;
extern unsigned int received_count;
extern uint16_t received_message_id;
extern uint8_t received_payload[16];
extern size_t received_payload_length;
extern bool fail_writes;
extern unsigned int failing_write_calls;
extern uint8_t uart_transmitted[CONFIG_COMM_ROUTER_MAX_PAYLOAD +
				BEAVER_PROTOCOL_FRAME_OVERHEAD];
extern size_t uart_transmitted_length;
extern uart_transport_receive_fn uart_receive;
extern void *uart_receive_context;
extern uint16_t commander_ack_message_id;
extern uint8_t commander_ack_status;
extern size_t commander_ack_length;
extern struct k_sem commander_ack_received;

extern struct comm_router_interface test_interface;

void reset_test_state(void *fixture);
int start_test_uart_transport(void);

#endif