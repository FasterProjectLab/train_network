#ifndef TELEMETRY_MANAGER_H
#define TELEMETRY_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_websocket_client.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t telemetry_manager_task_init(uint32_t interval_ms, esp_websocket_client_handle_t client);

void telemetry_manager_set_enabled(bool state, cJSON* payload);

bool telemetry_manager_is_active(void);

#ifdef __cplusplus
}
#endif

#endif 