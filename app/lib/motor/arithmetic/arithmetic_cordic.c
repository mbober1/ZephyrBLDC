#include <stm32_ll_bus.h>
#include <stm32_ll_cordic.h>
#include <stdint.h>
#include <zephyr/init.h>

#include "arithmetic.h"

#define HALF_TURN_DEG 180.0f
#define FULL_TURN_DEG 360.0f
#define Q31_SCALE (UINT32_C(1) << 31)


static int arithmetic_init(void)
{
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CORDIC);

	LL_CORDIC_Config(CORDIC, 
		LL_CORDIC_FUNCTION_COSINE, 
		LL_CORDIC_PRECISION_6CYCLES, // 4 iterations per cycle, 24 iterations total
		LL_CORDIC_SCALE_0,           // Result scaling not applicable
		LL_CORDIC_NBWRITE_1,         // One 32 bit input for angle. No modulus argument for sin and cos.
		LL_CORDIC_NBREAD_2,            // Two output data - sine then cosine
		LL_CORDIC_INSIZE_32BITS,      // q1.31 format for input data
		LL_CORDIC_OUTSIZE_32BITS);     // q1.31 format for output data
	return 0;
}

SYS_INIT(arithmetic_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

void arithmetic_sin_cos(float angle, float *sin, float *cos)
{
	const float to_q31 = (float)INT32_MAX / HALF_TURN_DEG;
	const float to_float = 1.0f / (float)Q31_SCALE;

	if (angle >= HALF_TURN_DEG) {
		angle -= FULL_TURN_DEG;
	} else if (angle < -HALF_TURN_DEG) {
		angle += FULL_TURN_DEG;
	}

	const int32_t cordic_angle = (int32_t)(angle * to_q31);

	LL_CORDIC_WriteData(CORDIC, (uint32_t)cordic_angle);
	while (!LL_CORDIC_IsActiveFlag_RRDY(CORDIC)) {
	}
	*cos = (int32_t)LL_CORDIC_ReadData(CORDIC) * to_float;
	*sin = (int32_t)LL_CORDIC_ReadData(CORDIC) * to_float;
}