#include <arm_math.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <spinner/drivers/currsmp.h>
#include <spinner/drivers/svpwm.h>
#include <spinner/drivers/feedback.h>

#include "arithmetic.h"

#define VOLTAGE_AMPLITUDE 0.3f
#define FULL_TURN_DEG 360.0f
#define ANGLE_STEP_DEGREES 5.0f
#define STEP_PERIOD_MS 1

const struct device *currsmp = DEVICE_DT_GET(DT_NODELABEL(currsmp));
const struct device *feedback = DEVICE_DT_GET(DT_NODELABEL(feedback));
struct currsmp_curr curr;
static volatile float feedback_rpm;
static volatile float feedback_angle;

static void noop_regulation_callback(void *ctx)
{
  feedback_angle = feedback_get_angle(feedback);
  feedback_rpm = feedback_get_speed(feedback);
  currsmp_get_currents(currsmp, &curr);
  (void)ctx;
}

int main(void)
{
  const struct device *svpwm = DEVICE_DT_GET(DT_NODELABEL(svpwm));
  float angle = 0.0f;

  if (!device_is_ready(currsmp) || !device_is_ready(svpwm)) {
    return 0;
  }

	if (!device_is_ready(feedback)) {
		return 0;
	}

  currsmp_configure(currsmp, noop_regulation_callback, NULL);
  currsmp_start(currsmp);
  svpwm_start(svpwm);

  while (1) {
    float sine;
    float cosine;

    arithmetic_sin_cos(angle, &sine, &cosine);
    svpwm_set_phase_voltages(svpwm, VOLTAGE_AMPLITUDE * cosine, 
                                    VOLTAGE_AMPLITUDE * sine);

    angle += ANGLE_STEP_DEGREES;
    if (angle >= FULL_TURN_DEG) {
      angle -= FULL_TURN_DEG;
    }

    k_msleep(STEP_PERIOD_MS);
  }

  return 0;
}