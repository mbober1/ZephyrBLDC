#ifndef APP_LIB_MOTOR_CALIBRATION_H_
#define APP_LIB_MOTOR_CALIBRATION_H_

#include <stddef.h>
#include <zephyr/device.h>

float get_position(const struct device *feedback, size_t samples);
void simple_move(const struct device *motor, size_t steps);
void energize_motor(const struct device *motor);
void deenergize_motor(const struct device *motor);

void calibration_regulation_cb(void *ctx);
float measure_pole_pairs(const struct device *motor, const struct device *feedback, size_t steps);
float measure_phase_offset(const struct device *motor, const struct device *feedback,
			   int pole_pairs);
float measure_phase_resistance(const struct device *motor, const struct device *currsmp,
			       float bus_voltage, size_t samples);

#endif
