#include <stdint.h>
#include <stdlib.h>

#include <zephyr/ztest.h>

#include "arithmetic.h"

#define Q31_QUARTER_TURN INT32_C(0x40000000)
#define Q31_HALF_TURN INT32_MIN
#define Q31_TOLERANCE 1000000

#define ASSERT_Q31_NEAR(actual, expected) \
	zassert_true(llabs((long long)(actual) - (expected)) < Q31_TOLERANCE, NULL)

ZTEST(arithmetic, test_cardinal_angles)
{
	int32_t sine;
	int32_t cosine;

	arithmetic_sin_cos_q31(0, &sine, &cosine);
	ASSERT_Q31_NEAR(sine, 0);
	ASSERT_Q31_NEAR(cosine, INT32_MAX);

	arithmetic_sin_cos_q31(Q31_QUARTER_TURN, &sine, &cosine);
	ASSERT_Q31_NEAR(sine, INT32_MAX);
	ASSERT_Q31_NEAR(cosine, 0);

	arithmetic_sin_cos_q31(Q31_HALF_TURN, &sine, &cosine);
	ASSERT_Q31_NEAR(sine, 0);
	ASSERT_Q31_NEAR(cosine, INT32_MIN);
}

ZTEST(arithmetic, test_negative_and_wrapped_angles)
{
	int32_t sine;
	int32_t cosine;

	arithmetic_sin_cos_q31(-Q31_QUARTER_TURN, &sine, &cosine);
	ASSERT_Q31_NEAR(sine, INT32_MIN);
	ASSERT_Q31_NEAR(cosine, 0);

	arithmetic_sin_cos_q31(Q31_QUARTER_TURN, &sine, &cosine);
	ASSERT_Q31_NEAR(sine, INT32_MAX);
	ASSERT_Q31_NEAR(cosine, 0);
}

ZTEST_SUITE(arithmetic, NULL, NULL, NULL, NULL, NULL);