#include "arithmetic.h"
#include "voltage_control.h"

#define VOLTAGE_AMPLITUDE 0.3f
#define FULL_TURN_DEG 360.0f
#define MICROSECONDS_PER_SECOND_F 1000000.0f

void motor_voltage_control(const float angle, float *sine, float *cosine)
{
	static float motor_angle;
	const float angle_increment = angle * FULL_TURN_DEG *
		(float)CONFIG_MOTOR_LOOP_PERIOD_US / MICROSECONDS_PER_SECOND_F;

	motor_angle += angle_increment;
	if (motor_angle >= FULL_TURN_DEG) {
		motor_angle -= FULL_TURN_DEG;
	} else if (motor_angle < 0.0f) {
		motor_angle += FULL_TURN_DEG;
	}

	arithmetic_sin_cos(motor_angle, sine, cosine);
	*sine *= VOLTAGE_AMPLITUDE;
	*cosine *= VOLTAGE_AMPLITUDE;
}
