#ifndef MOTOR_MANAGER_H
#define MOTOR_MANAGER_H

#include <stdint.h>
#include "esp_err.h"

esp_err_t motor_manager_pca9685_init(void);
esp_err_t motor_manager_pca9685_set_pwm(uint8_t channel, uint16_t duty);
void motor_manager_drv8833_set_speed(int16_t speed);
void motor_manager_drv8833_set_speed_percent(int8_t percent);

#endif // MOTOR_MANAGER_H