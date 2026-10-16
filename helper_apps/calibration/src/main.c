#include <math.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#include <spinner/drivers/currsmp.h>
#include <spinner/drivers/svpwm.h>

#include "motor_params.h"
#include "calibration.h"

#define BUS_VOLTAGE_V    (9.0f)
#define RESISTANCE_SAMPLES (50)
#define POLE_PAIR_STEPS (1000)
#define POLE_PAIR_MAX (40)

void calibration_regulation_cb(void *ctx) {}

int main(void)
{
	const struct device *motor = DEVICE_DT_GET(DT_NODELABEL(svpwm));
	const struct device *currsmp = DEVICE_DT_GET(DT_NODELABEL(currsmp));
	const struct device *feedback = DEVICE_DT_GET(DT_NODELABEL(feedback));
	float pole_pairs;
	int pole_pairs_int;
	float offset;
	int offset_int;
	float resistance;
	float inductance;
	enum motor_direction dir;
	struct motor_params params;

	if (!device_is_ready(motor) || !device_is_ready(currsmp) ||
	    !device_is_ready(feedback)) {
		printf("Calibration devices are not ready\n");
		return 0;
	}

	currsmp_configure(currsmp, calibration_regulation_cb, (void *)currsmp);
	currsmp_start(currsmp);
	svpwm_start(motor);
	
	resistance = measure_phase_resistance(motor, currsmp, BUS_VOLTAGE_V, RESISTANCE_SAMPLES);
	if (isnan(resistance)) {
		printf("Phase resistance measurement failed\n");
		return 0;
	}
	else
	{
		printf("phase resistance: %.4f ohm\n", (double)resistance);
	}

	pole_pairs = measure_pole_pairs(motor, feedback, POLE_PAIR_STEPS);
	if (isnan(pole_pairs)) {
		printf("Pole pairs measurement failed\n");
		return 0;
	}
	else if (pole_pairs < 0.0f) {
		pole_pairs *= -1.0f;
		dir = MOTOR_DIR_CLOCKWISE;
	} else {
		dir = MOTOR_DIR_COUNTERCLOCKWISE;
	}
	pole_pairs_int = (int)(pole_pairs + 0.5f);

	if (pole_pairs_int > POLE_PAIR_MAX) {
		printf("Pole pairs measurement failed\n");
		return 0;
	}
	else
	{
		printf("pole pairs: %d (%.3f)\n", pole_pairs_int, (double)pole_pairs);
	}


	offset = measure_phase_offset(motor, feedback, pole_pairs_int);
	if (isnan(offset)) {
		printf("Phase offset measurement failed\n");
		return 0;
	}
	else
	{
		offset_int = (int)(offset + 0.5f);
		printf("phase offset: %d (%.2f deg)\n", offset_int, (double)offset);
	}

	deenergize_motor(motor);


	if (settings_subsys_init() != 0) {
		printf("Settings init failed\n");
		return 0;
	}

	params.pole_pairs = pole_pairs_int;
	params.phase_offset_deg = offset_int;
	params.dir = dir;
	if (motor_params_save(&params) != 0) {
		printf("Calibration save failed\n");
	}
	else {
		printf("Calibration saved\n");
	}


	return 0;
}
