#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <spinner/drivers/currsmp.h>
#include <spinner/drivers/feedback.h>
#include <spinner/drivers/svpwm.h>

#include "motor.h"
#include "arithmetic.h"
#include "motor_params.h"
#include "voltage_control.h"

#if defined(CONFIG_COMMANDER)
#include "commander.h"
#endif

LOG_MODULE_REGISTER(motor, LOG_LEVEL_INF);

#define RPM_TO_REVOLUTIONS_PER_SECOND (1.0f / 60.0f)

static const struct device *const motor_currsmp = DEVICE_DT_GET(DT_NODELABEL(currsmp));
static const struct device *const motor_feedback = DEVICE_DT_GET(DT_NODELABEL(feedback));
static const struct device *const motor_svpwm = DEVICE_DT_GET(DT_NODELABEL(svpwm));
static struct currsmp_curr motor_current;

volatile static float speed;
volatile static float angle;

static void motor_regulation_callback(void *ctx)
{
	(void)ctx;
	currsmp_get_currents(motor_currsmp, &motor_current);
}

void motor_loop(void *p1, void *p2, void *p3)
{
	int64_t next_wakeup_ticks;
	static float sin, cos;
	struct motor_params params;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	__ASSERT(device_is_ready(motor_currsmp), "Motor current sampler is not ready");
	__ASSERT(device_is_ready(motor_feedback), "Motor feedback device is not ready");
	__ASSERT(device_is_ready(motor_svpwm), "Motor SVPWM device is not ready");

	if (motor_params_load(&params) != 0) {
		LOG_ERR("Failed to load motor parameters, run calibration");
		return;
	}

	currsmp_configure(motor_currsmp, motor_regulation_callback, NULL);
	currsmp_start(motor_currsmp);
	svpwm_start(motor_svpwm);

	#if defined(CONFIG_COMMANDER)
	struct commander_command command;
	next_wakeup_ticks = k_uptime_ticks();

	while (true) {
		commander_get(&command);

		switch (command.mode) {
		case COMMANDER_MODE_VOLTAGE:
			motor_voltage_control(command.setpoint * RPM_TO_REVOLUTIONS_PER_SECOND *
				(float)params.pole_pairs, &sin, &cos);
			break;



		default:
		case COMMANDER_MODE_OFF:
			sin = 0;
			cos = 0;
			break;
		}

		svpwm_set_phase_voltages(motor_svpwm, cos, sin);
		angle = feedback_get_angle(motor_feedback);
		speed = feedback_get_speed(motor_feedback);
		next_wakeup_ticks += K_USEC(CONFIG_MOTOR_LOOP_PERIOD_US).ticks;
		k_sleep(K_TIMEOUT_ABS_TICKS(next_wakeup_ticks));
	}
#endif
}
