#include <zephyr/ztest.h>

#include "commander.h"
#include "test_comm_router_support.h"

ZTEST_SUITE(commander, NULL, NULL, reset_test_state, NULL, NULL);

ZTEST(commander, test_commander_accepts_command_and_acknowledges)
{
	const uint8_t payload[COMMANDER_COMMAND_PAYLOAD_SIZE] = {
		COMMANDER_MODE_SPEED, 0x33U, 0x33U, 0x33U, 0xF3U, 0x00U, 0x80U,
	};
	struct commander_command command;

	zassert_equal(start_test_uart_transport(), 0);
	zassert_equal(comm_router_receive(&test_interface, COMMANDER_COMMAND_MESSAGE_ID,
					 payload, sizeof(payload)), 0);
	zassert_equal(k_sem_take(&commander_ack_received, K_SECONDS(1)), 0);
	zassert_equal(commander_ack_message_id, COMMANDER_ACK_MESSAGE_ID);
	zassert_equal(commander_ack_length, COMMANDER_ACK_PAYLOAD_SIZE);
	zassert_equal(commander_ack_status, COMMANDER_ACK_ACCEPTED);
	zassert_true(commander_get(&command));
	zassert_equal(command.mode, COMMANDER_MODE_SPEED);
	zassert_equal(command.setpoint, (int32_t)0xF3333333);
	zassert_equal(command.current, INT16_MIN);
}

ZTEST(commander, test_commander_acknowledges_invalid_commands)
{
	const uint8_t short_payload[COMMANDER_COMMAND_PAYLOAD_SIZE - 1U] = {0U};
	const uint8_t voltage_payload[COMMANDER_COMMAND_PAYLOAD_SIZE] = {
		COMMANDER_MODE_VOLTAGE, 0U, 0U, 0U, 0U,
	};
	const uint8_t unsupported_mode_payload[COMMANDER_COMMAND_PAYLOAD_SIZE] = {
		COMMANDER_MODE_MAX, 0U, 0U, 0U, 0U,
	};
	const uint8_t invalid_mode_payload[COMMANDER_COMMAND_PAYLOAD_SIZE] = {
		0xFFU, 0U, 0U, 0U, 0U,
	};
	struct commander_command command;

	zassert_equal(start_test_uart_transport(), 0);
	zassert_equal(comm_router_receive(&test_interface, COMMANDER_COMMAND_MESSAGE_ID,
					 short_payload, sizeof(short_payload)), 0);
	zassert_equal(k_sem_take(&commander_ack_received, K_SECONDS(1)), 0);
	zassert_equal(commander_ack_status, COMMANDER_ACK_INVALID_LENGTH);
	zassert_equal(commander_ack_length, COMMANDER_ACK_PAYLOAD_SIZE);

	zassert_equal(comm_router_receive(&test_interface, COMMANDER_COMMAND_MESSAGE_ID,
						 voltage_payload, sizeof(voltage_payload)), 0);
	zassert_equal(k_sem_take(&commander_ack_received, K_SECONDS(1)), 0);
	zassert_equal(commander_ack_status, COMMANDER_ACK_ACCEPTED);
	zassert_true(commander_get(&command));
	zassert_equal(command.mode, COMMANDER_MODE_VOLTAGE);

	zassert_equal(comm_router_receive(&test_interface, COMMANDER_COMMAND_MESSAGE_ID,
					 unsupported_mode_payload, sizeof(unsupported_mode_payload)), 0);
	zassert_equal(k_sem_take(&commander_ack_received, K_SECONDS(1)), 0);
	zassert_equal(commander_ack_status, COMMANDER_ACK_INVALID_MODE);
	zassert_true(commander_get(&command));
	zassert_equal(command.mode, COMMANDER_MODE_VOLTAGE);

	zassert_equal(comm_router_receive(&test_interface, COMMANDER_COMMAND_MESSAGE_ID,
					 invalid_mode_payload, sizeof(invalid_mode_payload)), 0);
	zassert_equal(k_sem_take(&commander_ack_received, K_SECONDS(1)), 0);
	zassert_equal(commander_ack_status, COMMANDER_ACK_INVALID_MODE);
	zassert_equal(commander_ack_length, COMMANDER_ACK_PAYLOAD_SIZE);
}