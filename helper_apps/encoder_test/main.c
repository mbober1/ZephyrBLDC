/*
 * Copyright (c) 2025 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/sys/printk.h>

#define ENCODER_BUFFER_SIZE 32

SENSOR_DT_STREAM_IODEV(encoder_iodev, DT_ALIAS(encoder0), {SENSOR_TRIG_TIMER, SENSOR_STREAM_DATA_INCLUDE});
RTIO_DEFINE_WITH_MEMPOOL(encoder_rtio, 4, 4, 4, ENCODER_BUFFER_SIZE, 4);

static const struct sensor_decoder_api *encoder_decoder;
static int64_t microdegrees;

static void encoder_process_result(int result, uint8_t *buffer)
{
	struct sensor_q31_data data;
	struct sensor_decode_context context = SENSOR_DECODE_CONTEXT_INIT(
		encoder_decoder, buffer, SENSOR_CHAN_ROTATION, 0);
	int decoded;

	if (result != 0) {
		printk("Encoder RTIO read failed: %d\n", result);
		return;
	}

	decoded = sensor_decode(&context, &data, 1);
	if (decoded < 0) {
		printk("Encoder decode failed: %d\n", decoded);
		return;
	}

	microdegrees = ((int64_t)data.readings[0].angle *
				       (1LL << data.shift) * 1000000LL) /
				      (1LL << 31);

	printk("angle=%lld.%06lld deg\n", microdegrees / 1000000LL,
	       microdegrees % 1000000LL);
}

int main(void)
{
	static const struct device *const encoder = DEVICE_DT_GET(DT_ALIAS(encoder0));
	int ret;

	if (!device_is_ready(encoder)) {
		printk("Encoder device is not ready\n");
		return 0;
	}

	ret = sensor_get_decoder(encoder, &encoder_decoder);
	if (ret < 0) {
		printk("Encoder decoder is unavailable: %d\n", ret);
		return 0;
	}

	ret = sensor_stream(&encoder_iodev, &encoder_rtio, NULL, NULL);
	if (ret < 0) {
		printk("Encoder stream start failed: %d\n", ret);
		return 0;
	}

	while (true) {
		struct rtio_cqe *cqe = rtio_cqe_consume_block(&encoder_rtio);
		uint8_t *buffer = NULL;
		uint32_t buffer_len = 0;

		if (cqe->result == 0) {
			ret = rtio_cqe_get_mempool_buffer(&encoder_rtio, cqe, &buffer, &buffer_len);
			if (ret < 0 || buffer == NULL || buffer_len < ENCODER_BUFFER_SIZE) {
				printk("Encoder stream buffer unavailable: %d\n", ret);
			} else {
				encoder_process_result(cqe->result, buffer);
			}
		}

		if (buffer != NULL) {
			rtio_release_buffer(&encoder_rtio, buffer, buffer_len);
		}
		rtio_cqe_release(&encoder_rtio, cqe);
	}

	return 0;
}
