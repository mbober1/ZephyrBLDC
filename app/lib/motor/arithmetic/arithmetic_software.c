#include <math.h>

#include "arithmetic.h"

#define DEG_TO_RAD (3.14159265358979323846f / 180.0f)

void arithmetic_sin_cos(float angle, float *sin, float *cos)
{
	const float radians = angle * DEG_TO_RAD;

	*sin = sinf(radians);
	*cos = cosf(radians);
}