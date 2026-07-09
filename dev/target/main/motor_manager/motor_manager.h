#ifndef MOTOR_MANAGER_H
#define MOTOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void motor_manager_init(void);

void motor_manager_set_speed(uint8_t duty, bool forward);

void motor_manager_stop(void);

void motor_manager_set_direction(bool forward);

bool motor_manager_get_current_direction(void);

uint8_t motor_manager_get_current_speed(void);

#ifdef __cplusplus
}
#endif

#endif 