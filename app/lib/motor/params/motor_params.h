#ifndef APP_LIB_MOTOR_PARAMS_H_
#define APP_LIB_MOTOR_PARAMS_H_

enum motor_direction {
	MOTOR_DIR_CLOCKWISE,
	MOTOR_DIR_COUNTERCLOCKWISE,
};

struct motor_params {
	int pole_pairs;
	int phase_offset_deg;
	enum motor_direction dir;
};

int motor_params_load(struct motor_params *params);
int motor_params_save(const struct motor_params *params);

#endif
