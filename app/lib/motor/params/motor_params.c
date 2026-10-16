#include <errno.h>
#include <stdint.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/crc.h>

#include "motor_params.h"

LOG_MODULE_REGISTER(motor_params, LOG_LEVEL_INF);

#define MOTOR_PARAMS_KEY "motor/params"

struct motor_params_record {
	struct motor_params params;
	uint32_t crc;
};

static uint32_t params_crc32(const struct motor_params *params)
{
	return crc32_ieee((const uint8_t *)params, sizeof(*params));
}

int motor_params_load(struct motor_params *params)
{
	struct motor_params_record record;
	int len;
	int ret;

	ret = settings_subsys_init();
	if (ret != 0) {
		LOG_ERR("Settings init failed: %d", ret);
		return ret;
	}

	len = settings_load_one(MOTOR_PARAMS_KEY, &record, sizeof(record));
	if (len < 0) {
		LOG_ERR("motor/params not found in NVS (%d), run calibration", len);
		return len;
	}

	if (len != sizeof(record)) {
		LOG_ERR("motor/params size mismatch (%d), run calibration", len);
		return -EINVAL;
	}

	if (record.crc != params_crc32(&record.params)) {
		LOG_ERR("motor params checksum mismatch, run calibration");
		return -EINVAL;
	}

	*params = record.params;
	return 0;
}

int motor_params_save(const struct motor_params *params)
{
	struct motor_params_record record = {.params = *params};

	record.crc = params_crc32(&record.params);
	return settings_save_one(MOTOR_PARAMS_KEY, &record, sizeof(record));
}
