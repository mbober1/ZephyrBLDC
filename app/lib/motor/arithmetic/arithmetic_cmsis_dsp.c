#include <arm_math.h>
#include "arithmetic.h"

void arithmetic_sin_cos(float angle, float *sin, float *cos)
{
	arm_sin_cos_f32(angle, sin, cos);
}