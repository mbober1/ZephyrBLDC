/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <spinner/drivers/feedback.h>


int main(void)
{
	const struct device *feedback = DEVICE_DT_GET(DT_NODELABEL(feedback));

  static float angle;
  static float speed;
  static float position;

	if (!device_is_ready(feedback)) {
		printk("Feedback device is not ready\n");
		return 0;
	}

	printk("RTIO feedback test started\n");
	while (true) {
		angle = feedback_get_angle(feedback);
		speed = feedback_get_speed(feedback);
		position = feedback_get_position(feedback);
		// printk("eangle=%.3f deg speed=%.3f rpm position=%.3f deg\n", angle, speed, position);
		k_msleep(1);
	}

	return 0;
}