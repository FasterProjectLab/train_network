#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t camera_manager_init_hardware(void);

void camera_manager_task_init(void);

void camera_manager_set_enabled(bool state);

void camera_manager_update_settings(const char* variable, int value);

uint32_t camera_manager_generate_ssrc_from_mac(void);

bool camera_manager_is_active(void);

#ifdef __cplusplus
}
#endif

#endif 