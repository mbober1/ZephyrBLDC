#include <stdint.h>
#include <math.h>

#include <zephyr/ztest.h>

#include "arithmetic.h"

#define PI_F 3.14159265358979323846f
#define HALF_PI_F (PI_F / 2.0f)
#define FLOAT_TOLERANCE 0.001f

#define ASSERT_FLOAT_NEAR(actual, expected) \
	zassert_true(fabsf((actual) - (expected)) < FLOAT_TOLERANCE, NULL)

ZTEST(arithmetic, test_cardinal_angles)
{
	float sine;
	float cosine;

	arithmetic_sin_cos(0.0f, &sine, &cosine);
	ASSERT_FLOAT_NEAR(sine, 0.0f);
	ASSERT_FLOAT_NEAR(cosine, 1.0f);

	arithmetic_sin_cos(HALF_PI_F, &sine, &cosine);
	ASSERT_FLOAT_NEAR(sine, 1.0f);
	ASSERT_FLOAT_NEAR(cosine, 0.0f);

	arithmetic_sin_cos(PI_F, &sine, &cosine);
	ASSERT_FLOAT_NEAR(sine, 0.0f);
	ASSERT_FLOAT_NEAR(cosine, -1.0f);
}

ZTEST(arithmetic, test_negative_and_wrapped_angles)
{
	float sine;
	float cosine;

	arithmetic_sin_cos(-HALF_PI_F, &sine, &cosine);
	ASSERT_FLOAT_NEAR(sine, -1.0f);
	ASSERT_FLOAT_NEAR(cosine, 0.0f);

	arithmetic_sin_cos(5.0f * HALF_PI_F, &sine, &cosine);
	ASSERT_FLOAT_NEAR(sine, 1.0f);
	ASSERT_FLOAT_NEAR(cosine, 0.0f);
}

ZTEST_SUITE(arithmetic, NULL, NULL, NULL, NULL, NULL);