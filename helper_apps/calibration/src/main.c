#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include <spinner/drivers/currsmp.h>
#include <spinner/drivers/svpwm.h>

#include "calibration.h"

#define BUS_VOLTAGE_V    (9.0f)
#define RESISTANCE_SAMPLES (50)
#define POLE_PAIR_STEPS (1000)

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

	if (!device_is_ready(motor) || !device_is_ready(currsmp) ||
	    !device_is_ready(feedback)) {
		printf("Calibration devices are not ready\n");
		return 0;
	}

	currsmp_configure(currsmp, calibration_regulation_cb, (void *)currsmp);
	currsmp_start(currsmp);
	svpwm_start(motor);

	pole_pairs = measure_pole_pairs(motor, feedback, POLE_PAIR_STEPS);
	pole_pairs_int = (int)(pole_pairs + 0.5f);
	printf("pole pairs: %d (%.3f)\n", pole_pairs_int, (double)pole_pairs);

	offset = measure_phase_offset(motor, feedback, 11);
	offset_int = (int)(offset + 0.5f);
	printf("phase offset: %d (%.2f deg)\n", offset_int, (double)offset);

	resistance = measure_phase_resistance(motor, currsmp, BUS_VOLTAGE_V, RESISTANCE_SAMPLES);
	printf("phase resistance: %.4f ohm\n", (double)resistance);

	// inductance = measure_phase_inductance(motor, resistance, vbus);


	// svpwm_set_phase_voltages(motor, 0.3f, 0);
	// static volatile struct currsmp_curr curr;
	// while (1) {
  //   currsmp_get_currents(currsmp, &curr);
	// }
	deenergize_motor(motor);

	// printf("phase inductance: %.6f H\n", (double)inductance);

	return 0;
}
