#ifndef COMMANDER_H
#define COMMANDER_H

#include <stdbool.h>
#include <stdint.h>

#define COMMANDER_COMMAND_MESSAGE_ID 		(0x0100U)
#define COMMANDER_ACK_MESSAGE_ID 				(0x0101U)
#define COMMANDER_COMMAND_PAYLOAD_SIZE 	(7U)
#define COMMANDER_ACK_PAYLOAD_SIZE 			(1U)

enum commander_mode {
	COMMANDER_MODE_OFF,
	COMMANDER_MODE_VOLTAGE,
	COMMANDER_MODE_TORQUE,
	COMMANDER_MODE_SPEED,
	COMMANDER_MODE_POSITION,
	COMMANDER_MODE_CALIBRATION,
	COMMANDER_MODE_MAX,
};

enum commander_ack_status {
	COMMANDER_ACK_ACCEPTED,
	COMMANDER_ACK_INVALID_LENGTH,
	COMMANDER_ACK_INVALID_MODE,
};

struct commander_command {
	enum commander_mode mode;
	int32_t setpoint; /* Q31 mechanical revolutions per second. */
	int16_t current;
};

bool commander_get(struct commander_command *command);

#endif