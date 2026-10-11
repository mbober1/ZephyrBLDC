#include <math.h>

#include "arithmetic.h"

void arithmetic_sin_cos(float angle, float *sin, float *cos)
{
	*sin = sinf(angle);
	*cos = cosf(angle);
}