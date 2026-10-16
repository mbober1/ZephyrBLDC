#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>

#include <spinner/drivers/currsmp.h>
#include <spinner/drivers/feedback.h>
#include <spinner/drivers/svpwm.h>

#include "arithmetic.h"
#include "calibration.h"

LOG_MODULE_REGISTER(calibration, LOG_LEVEL_INF);

#define MOTOR_STEP_PERIOD_MS              (2)
#define MOTOR_STEP_DEGREES                (3.0f)
#define CALIBRATION_VOLTAGE               (0.3f)
#define CALIBRATION_FULL_TURN_DEG         (360.0f)
#define CALIBRATION_SETTLE_MS             (1000)
#define CALIBRATION_POSITION_SAMPLES      (20)
#define CALIBRATION_OFFSET_STEPS_PER_REV  (20)
#define CALIBRATION_OFFSET_STEP_DEGREES   (CALIBRATION_FULL_TURN_DEG / CALIBRATION_OFFSET_STEPS_PER_REV)
#define CALIBRATION_OFFSET_SAMPLES        (4)
#define CALIBRATION_OFFSET_SETTLE_MS      (20)
#define CALIBRATION_MIN_CURRENT           (0.100f)
#define RAD_TO_DEG                        (180.0f / 3.14159265358979323846f)

float get_position(const struct device *const feedback, const size_t samples) {
  float position = 0.0f;

  if (samples == 0) {
    return NAN;
  }

  for (size_t i = 0; i < samples; ++i) {
    k_msleep(CONFIG_SPINNER_FEEDBACK_RTIO_SAMPLE_PERIOD_MS);
    position += feedback_get_position(feedback);
  }

  return position / (float)samples;
}

void simple_move(const struct device *const motor, const size_t steps) {
  float sine;
  float cosine;
  float angle = 0.0f;
  const float step = MOTOR_STEP_DEGREES;

  for (size_t i = 0; i < steps; ++i) {
    angle += step;
    if (angle >= CALIBRATION_FULL_TURN_DEG) {
      angle -= CALIBRATION_FULL_TURN_DEG;
    }

    arithmetic_sin_cos(angle, &sine, &cosine);

    svpwm_set_phase_voltages(motor, CALIBRATION_VOLTAGE * cosine,
                             CALIBRATION_VOLTAGE * sine);
    k_msleep(MOTOR_STEP_PERIOD_MS);
  }
}

void energize_motor(const struct device *const motor) {
  svpwm_set_phase_voltages(motor, CALIBRATION_VOLTAGE, 0.0f);
}

void deenergize_motor(const struct device *const motor) {
  svpwm_set_phase_voltages(motor, 0.0f, 0.0f);
}

static float wrap_degrees(float degrees) {
  degrees -= 360.0f * (float)(int32_t)(degrees / 360.0f);
  if (degrees < 0.0f) {
    degrees += 360.0f;
  }

  return degrees;
}

/* Returns signed pole pairs; a negative value means the encoder runs reversed. */
float measure_pole_pairs(const struct device *const motor,
                         const struct device *const feedback,
                         const size_t steps) {
  float start;
  float end;

  energize_motor(motor);
  k_msleep(CALIBRATION_SETTLE_MS);
  start = get_position(feedback, CALIBRATION_POSITION_SAMPLES);

  simple_move(motor, steps);
  k_msleep(CALIBRATION_SETTLE_MS);
  end = get_position(feedback, CALIBRATION_POSITION_SAMPLES);
  deenergize_motor(motor);

  if (end == start) {
    return NAN;
  }

  return ((float)steps * MOTOR_STEP_DEGREES) / (end - start);
}

/* Sweeps one electrical revolution (direction +1 forward, -1 reverse) and accumulates unit vectors. */
static void offset_sweep(const struct device *const motor,
                         const struct device *const feedback,
                         const int pole_pairs, const float direction,
                         float *const sum_sin, float *const sum_cos) {
  const size_t steps = (size_t)pole_pairs * CALIBRATION_OFFSET_STEPS_PER_REV;
  const float step = direction * CALIBRATION_OFFSET_STEP_DEGREES;
  float sine;
  float cosine;
  float angle = 0.0f;

  *sum_sin = 0.0f;
  *sum_cos = 0.0f;

  k_msleep(CALIBRATION_SETTLE_MS);

  for (size_t i = 0; i < steps; ++i) {
    angle += step;
    if (angle >= CALIBRATION_FULL_TURN_DEG) {
      angle -= CALIBRATION_FULL_TURN_DEG;
    } else if (angle < 0.0f) {
      angle += CALIBRATION_FULL_TURN_DEG;
    }

    arithmetic_sin_cos(angle, &sine, &cosine);
    svpwm_set_phase_voltages(motor, CALIBRATION_VOLTAGE * cosine,
                             CALIBRATION_VOLTAGE * sine);
    k_msleep(CALIBRATION_OFFSET_SETTLE_MS);

    const float position = get_position(feedback, CALIBRATION_OFFSET_SAMPLES);
    const float rotor_deg = (float)pole_pairs * position;
    arithmetic_sin_cos(wrap_degrees(angle - rotor_deg), &sine, &cosine);
    *sum_sin += sine;
    *sum_cos += cosine;
  }
}

static float sums_to_degrees(const float sum_sin, const float sum_cos) {
  return wrap_degrees(atan2f(sum_sin, sum_cos) * RAD_TO_DEG);
}

float measure_phase_offset(const struct device *const motor,
                           const struct device *const feedback,
                           const int pole_pairs) {
  float fwd_sin;
  float fwd_cos;
  float rev_sin;
  float rev_cos;
  float drag;

  if (pole_pairs <= 0) {
    return NAN;
  }

  energize_motor(motor);

  offset_sweep(motor, feedback, pole_pairs, 1.0f, &fwd_sin, &fwd_cos);
  offset_sweep(motor, feedback, pole_pairs, -1.0f, &rev_sin, &rev_cos);

  deenergize_motor(motor);

  float fwd_deg = sums_to_degrees(fwd_sin, fwd_cos);
  float rev_deg = sums_to_degrees(rev_sin, rev_cos);

  drag = wrap_degrees(fwd_deg - rev_deg);
  if (drag > 180.0f) {
    drag -= 360.0f;
  }
  LOG_INF("Phase offset drag: %.3f deg", (double)drag);

  return sums_to_degrees(fwd_sin + rev_sin, fwd_cos + rev_cos);
}

/* Drives a voltage vector at `angle` and returns the mean magnitude of phase `phase` (0=a, 1=b, 2=c). */
static float measure_phase_current(const struct device *const motor,
                                   const struct device *const currsmp,
                                   const float angle, const int phase,
                                   const size_t samples) {
  struct currsmp_curr curr;
  float current = 0.0f;
  float sine;
  float cosine;

  arithmetic_sin_cos(angle, &sine, &cosine);
  svpwm_set_phase_voltages(motor, CALIBRATION_VOLTAGE * cosine,
                           CALIBRATION_VOLTAGE * sine);
  k_msleep(CALIBRATION_SETTLE_MS);

  for (size_t i = 0; i < samples; ++i) {
    currsmp_get_currents(currsmp, &curr);
    current += (phase == 0) ? curr.i_a : (phase == 1) ? curr.i_b : curr.i_c;
    k_msleep(1);
  }

  deenergize_motor(motor);

  return fabsf(current / (float)samples);
}

float measure_phase_resistance(const struct device *const motor,
                               const struct device *const currsmp,
                               const float bus_voltage,
                               const size_t samples) {
  float sum = 0.0f;

  if (samples == 0 || currsmp == NULL) {
    return NAN;
  }

  for (int phase = 0; phase < 3; ++phase) {
    const float angle = (float)phase * CALIBRATION_FULL_TURN_DEG / 3.0f;
    const float current = measure_phase_current(motor, currsmp, angle, phase, samples);

    LOG_INF("Phase %d average current: %.4f A", phase, (double)current);
    if (current == 0.0f) {
      return NAN;
    }
    else if (current < CALIBRATION_MIN_CURRENT) {
      return NAN;
    }
    
    sum += (2.0f / 3.0f) * CALIBRATION_VOLTAGE * bus_voltage / current;
  }

  return sum / 3.0f;
}
