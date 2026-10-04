#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "commander.h"

LOG_MODULE_REGISTER(commander, LOG_LEVEL_INF);

void commander_loop(void)
{
	while (true) {
		LOG_INF("Running commander loop iteration");
		k_sleep(K_SECONDS(1));
	}
}