#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/byteorder.h>
#include "commander.h"
#include "comm_router.h"

LOG_MODULE_REGISTER(commander, LOG_LEVEL_INF);


struct commander_event {
	enum commander_ack_status status;
	struct commander_command command;
};

enum commander_payload_offset {
	PAYLOAD_MODE_OFFSET = 0U,
	PAYLOAD_SETPOINT_OFFSET = PAYLOAD_MODE_OFFSET + sizeof(uint8_t),
	PAYLOAD_CURRENT_OFFSET = PAYLOAD_SETPOINT_OFFSET + sizeof(int32_t),
};


static struct commander_command latest_command = {
	.mode = COMMANDER_MODE_OFF,
	.setpoint = 0,
	.current = 0,
};

static void commander_work_handler(struct k_work *work);
K_MSGQ_DEFINE(commander_event_queue, sizeof(struct commander_event), CONFIG_COMMANDER_EVENT_QUEUE_SIZE, 4);
static K_MUTEX_DEFINE(latest_command_mutex);
static K_WORK_DEFINE(commander_work, commander_work_handler);

static enum commander_ack_status decode_command(const uint8_t *payload, size_t payload_length,
						struct commander_command *command)
{
	if (payload == NULL || payload_length != COMMANDER_COMMAND_PAYLOAD_SIZE) {
		return COMMANDER_ACK_INVALID_LENGTH;
	}

	if (payload[PAYLOAD_MODE_OFFSET] >= COMMANDER_MODE_MAX) {
		return COMMANDER_ACK_INVALID_MODE;
	}

	command->mode = (enum commander_mode)payload[PAYLOAD_MODE_OFFSET];
	command->setpoint = (int32_t)sys_get_le32(&payload[PAYLOAD_SETPOINT_OFFSET]);
	command->current = sys_get_le16(&payload[PAYLOAD_CURRENT_OFFSET]);

	return COMMANDER_ACK_ACCEPTED;
}

static void receive_command(void *context, uint16_t message_id, const uint8_t *payload,
			    size_t payload_length)
{
	ARG_UNUSED(context);
	ARG_UNUSED(message_id);

	struct commander_event event = {0};
	event.status = decode_command(payload, payload_length, &event.command);

	if (k_msgq_put(&commander_event_queue, &event, K_NO_WAIT) != 0) {
		LOG_WRN("Dropping command event: commander queue is full");
		return;
	}

	(void)k_work_submit(&commander_work);
}

COMM_ROUTER_CUSTOMER_DEFINE(commander_command_customer, COMMANDER_COMMAND_MESSAGE_ID, receive_command, NULL);

static void send_ack(enum commander_ack_status status)
{
	const uint8_t ack_payload[COMMANDER_ACK_PAYLOAD_SIZE] = {(uint8_t)status};
	int result = comm_router_send(COMMANDER_ACK_MESSAGE_ID, ack_payload, sizeof(ack_payload));
	if (result != 0) {
		LOG_WRN("Failed to send command acknowledgement: %d", result);
	}
}

bool commander_get(struct commander_command *command)
{
	if (command == NULL) {
		return false;
	}

	k_mutex_lock(&latest_command_mutex, K_FOREVER);
	*command = latest_command;
	k_mutex_unlock(&latest_command_mutex);
	return true;
}

static void commander_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	struct commander_event event;

	while (k_msgq_get(&commander_event_queue, &event, K_NO_WAIT) == 0) {

		if (event.status == COMMANDER_ACK_ACCEPTED) {
			k_mutex_lock(&latest_command_mutex, K_FOREVER);
			latest_command = event.command;
			k_mutex_unlock(&latest_command_mutex);
		}

		send_ack(event.status);
	}
}