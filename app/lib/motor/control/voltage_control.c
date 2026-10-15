#include "arithmetic.h"
#include "voltage_control.h"

#define VOLTAGE_AMPLITUDE 0.3f
#define TWO_PI_F 6.28318530717958647692f
#define MICROSECONDS_PER_SECOND_F 1000000.0f

void motor_voltage_control(const float angle, float *sine, float *cosine)
{
	static float motor_angle;
	const float angle_increment = angle * TWO_PI_F *
		(float)CONFIG_MOTOR_LOOP_PERIOD_US / MICROSECONDS_PER_SECOND_F;

	motor_angle += angle_increment;
	if (motor_angle >= TWO_PI_F) {
		motor_angle -= TWO_PI_F;
	} else if (motor_angle < 0.0f) {
		motor_angle += TWO_PI_F;
	}

	arithmetic_sin_cos(motor_angle, sine, cosine);
	*sine *= VOLTAGE_AMPLITUDE;
	*cosine *= VOLTAGE_AMPLITUDE;
}
