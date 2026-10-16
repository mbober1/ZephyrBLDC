#ifndef CALIB_STORE_H_
#define CALIB_STORE_H_

#include <stdint.h>

enum direction {
	CLOCKWISE,
	COUNTERCLOCKWISE
};

struct calib_data {
	float resistance;
	int32_t pole_pairs;
	int32_t phase_offset;
	int32_t dir;
};

int calib_store_save(const struct calib_data *data);

#endif
