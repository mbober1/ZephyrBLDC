#include <stdint.h>

#include <zephyr/settings/settings.h>

#include "calib_store.h"

int calib_store_save(const struct calib_data *data)
{
	return settings_save_one("calib/data", data, sizeof(*data));
}
