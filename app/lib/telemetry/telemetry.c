#include <errno.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#include "comm_router.h"
#include "telemetry.h"

LOG_MODULE_REGISTER(telemetry, LOG_LEVEL_INF);

#define TELEMETRY_MESSAGE_ID 0x0200U
#define TELEMETRY_PERIOD_MS 1000

struct telemetry_payload {
	int16_t temperature;
	int16_t power;
	int16_t energy;
} __packed;

BUILD_ASSERT(sizeof(struct telemetry_payload) == 3U * sizeof(int16_t));

void telemetry_loop(void)
{
	LOG_INF("Starting telemetry");
	int64_t next_send_time_ms = k_uptime_get() + TELEMETRY_PERIOD_MS;

	const struct telemetry_payload values = {
		.temperature = 21,
		.power = 100,
		.energy = 50,
	};

	while (true) {
		k_sleep(K_TIMEOUT_ABS_MS(next_send_time_ms));
		next_send_time_ms += TELEMETRY_PERIOD_MS;
		
		int result = comm_router_send(TELEMETRY_MESSAGE_ID, (const uint8_t *)&values, sizeof(values));
		if (result != 0) {
			LOG_WRN("Failed to send telemetry: %d", result);
		}
	}
}
