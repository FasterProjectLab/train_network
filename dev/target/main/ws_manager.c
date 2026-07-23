#include "esp_websocket_client.h"
#include "main_config.h"
#include <string.h>
#include "cJSON.h"
#include <stdio.h>
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "camera_manager/camera_manager.h"
#include "telemetry_manager/telemetry_manager.h"
#include "ota_manager/ota_manager.h"

static char* get_ws_identifier(void);
static void send_ws_connection(esp_websocket_client_handle_t client);
static void handle_motor_command(cJSON *payload);
static void handle_light_command(cJSON *payload);
static void handle_system_command(cJSON *payload);
static void handle_camera_control(cJSON *payload);
static void process_incoming_message(const char *data, size_t len, esp_websocket_client_handle_t client);
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void get_mac_address(char *out_str, size_t size);
static void websocket_heartbeat_task(void *pvParameters);
void websocket_stop_heartbeat(void);
static void send_ws_train_status(esp_websocket_client_handle_t client, const char *target);

static TaskHandle_t heartbeat_task_handle = NULL;

static const char *TAG = "WS_MANAGER";

void websocket_app_start(void) {
    static char full_url[128];
    snprintf(full_url, sizeof(full_url), SERVER_WS_URL, get_ws_identifier());

    const esp_websocket_client_config_t ws_cfg = {
        .uri = full_url,
        .reconnect_timeout_ms = 10000,
        .network_timeout_ms = 10000,
        .buffer_size = 4096,
        .task_stack = 6144,
        .task_prio = 5,
    };

    camera_manager_task_init();

    ESP_LOGI(TAG, "Connecting to: %s", full_url);

    esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    esp_websocket_client_start(client);

    telemetry_manager_task_init(5000, client);
}

void send_ws_envelope(esp_websocket_client_handle_t client, const char* type, const char* target, cJSON* payload) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddStringToObject(root, "source", get_ws_identifier()); 
    cJSON_AddStringToObject(root, "target", target);
    cJSON_AddNumberToObject(root, "timestamp", (long)(esp_timer_get_time() / 1000));
    
    if (payload) {
        cJSON_AddItemToObject(root, "payload", payload);
    } else {
        cJSON_AddObjectToObject(root, "payload");
    }

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        esp_websocket_client_send_text(client, json_str, strlen(json_str), pdMS_TO_TICKS(1000));
        free(json_str);
    }
    
    cJSON_Delete(root);
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_client_handle_t client = (esp_websocket_client_handle_t)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket: Connected to transport");
            
            if (heartbeat_task_handle == NULL) {
                xTaskCreate(websocket_heartbeat_task, "ws_heartbeat", 3072, client, 3, &heartbeat_task_handle);
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->data_len > 0 && data->op_code == 0x01) {
                process_incoming_message((char *)data->data_ptr, data->data_len, client);
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket: Disconnected");
            websocket_stop_heartbeat();
            camera_manager_set_enabled(false);
            telemetry_manager_set_enabled(false, NULL);
            break;

        default:
            break;
    }
}

static void process_incoming_message(const char *data, size_t len, esp_websocket_client_handle_t client) {
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (root == NULL) return;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    cJSON *source = cJSON_GetObjectItem(root, "source");

    if (cJSON_IsString(type)) {
        const char *type_str = type->valuestring;

        if (strcmp(type_str, "welcom") == 0) {
            ESP_LOGI(TAG, "Server authentication successful");
            send_ws_connection(client);
        }
        else if (strcmp(type_str, "camera_start") == 0) {
            camera_manager_set_enabled(true);
        }
        else if (strcmp(type_str, "camera_stop") == 0) {
            camera_manager_set_enabled(false);
        }
        else if (strcmp(type_str, "camera_control") == 0) {
            handle_camera_control(payload);
        }
        else if (strcmp(type_str, "telemetry_start") == 0) {
            telemetry_manager_set_enabled(true, payload);
        }
        else if (strcmp(type_str, "telemetry_stop") == 0) {
            telemetry_manager_set_enabled(false, NULL);
        }
        else if (strcmp(type_str, "system") == 0) {
            handle_system_command(payload);
        }
        else if (strcmp(type_str, "motor") == 0) {
            handle_motor_command(payload);
        }
        else if (strcmp(type_str, "light") == 0) {
            handle_light_command(payload);
        } 
        else if (strcmp(type_str, "get_train_status") == 0) {
            const char *source_str = "server";
            send_ws_train_status(client, source_str);
        }
    }
    cJSON_Delete(root);
}

static void handle_motor_command(cJSON *payload) {
    if (!cJSON_IsObject(payload)) return;

    cJSON *speed_item = cJSON_GetObjectItem(payload, "speed");
    cJSON *dir_item = cJSON_GetObjectItem(payload, "dir");

    if (cJSON_IsString(speed_item)) {
        int speed_val = atoi(speed_item->valuestring);
        bool forward = true;

        if (cJSON_IsString(dir_item) && strcmp(dir_item->valuestring, "rev") == 0) {
            forward = false;
        }
        
        if (speed_val > 0) {
            //motor_manager_drv8833_set_speed(speed_val);
            ESP_LOGI(TAG, "Motor: Speed %d, Dir: %s", speed_val, forward ? "FWD" : "REV");
        } else {
            //motor_manager_drv8833_set_speed(0);
            //motor_manager_set_direction(forward);
        }
    }
}

static void handle_light_command(cJSON *payload) {
    // if (!cJSON_IsObject(payload)) return;

    // cJSON *status_item = cJSON_GetObjectItem(payload, "status");
    // if (!cJSON_IsString(status_item)) return;

    // if (strcmp(status_item->valuestring, "ON") == 0) {
    //     light_manager_update_by_current_dir();
    // } else {
    //     light_manager_set(false, false, false, false);
    // }
}

void light_manager_update_by_current_dir(void) {
    // if (motor_manager_get_current_direction()) {
    //     light_manager_set(false, false, true, true);
    // } else {
    //     light_manager_set(true, true, false, false);
    // }
}

static void handle_system_command(cJSON *payload) {
    if (!cJSON_IsObject(payload)) return;

    cJSON *action_item = cJSON_GetObjectItem(payload, "action");
    if (!cJSON_IsString(action_item)) return;

    if (strcmp(action_item->valuestring, "update_firmware") == 0) {
        cJSON *src_item = cJSON_GetObjectItem(payload, "src");
        if (cJSON_IsString(src_item)) {
            ESP_LOGW(TAG, "Starting OTA update from: %s", src_item->valuestring);
            start_ota_update(src_item->valuestring);
        }
    } 
    else if (strcmp(action_item->valuestring, "reboot") == 0) {
        ESP_LOGW(TAG, "Reboot command received. Restarting...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
}

static void handle_camera_control(cJSON *payload) {
    if (!cJSON_IsObject(payload)) return;

    cJSON *var_item = cJSON_GetObjectItem(payload, "variable");
    cJSON *val_item = cJSON_GetObjectItem(payload, "value");

    if (cJSON_IsString(var_item) && cJSON_IsNumber(val_item)) {
        camera_manager_update_settings(var_item->valuestring, val_item->valueint);
    }
}

static void send_ws_connection(esp_websocket_client_handle_t client) {
    if (esp_websocket_client_is_connected(client)) {
        cJSON *caps = cJSON_CreateObject();
        cJSON_AddStringToObject(caps, "device", get_ws_identifier());
        cJSON_AddNumberToObject(caps, "ssrc", (double) camera_manager_generate_ssrc_from_mac());
        cJSON_AddBoolToObject(caps, "camera", true);
        send_ws_envelope(client, "ack_welcom", "server", caps);
    }
}

static void websocket_heartbeat_task(void *pvParameters) {
    esp_websocket_client_handle_t client = (esp_websocket_client_handle_t)pvParameters;
    while (1) {
        if (esp_websocket_client_is_connected(client)) {
            send_ws_envelope(client, "ping", "server", NULL);
            ESP_LOGD(TAG, "Heartbeat sent");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void websocket_stop_heartbeat(void) {
    if (heartbeat_task_handle != NULL) {
        vTaskDelete(heartbeat_task_handle);
        heartbeat_task_handle = NULL;
        ESP_LOGI(TAG, "Heartbeat task stopped");
    }
}

void get_mac_address(char *out_str, size_t size) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out_str, size, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static char* get_ws_identifier(void) {
    static char mac_id[13];
    get_mac_address(mac_id, sizeof(mac_id));
    return mac_id;
}

static void send_ws_train_status(esp_websocket_client_handle_t client, const char *target) {
    if (!esp_websocket_client_is_connected(client)) return;

    cJSON *status_payload = cJSON_CreateObject();
    if (!status_payload) return;

    //cJSON_AddBoolToObject(status_payload, "light", light_manager_is_enabled());

    cJSON *motor_obj = cJSON_CreateObject();
    if (motor_obj) {      
        //bool forward = motor_manager_get_current_direction();
        
        //cJSON_AddNumberToObject(motor_obj, "speed", motor_manager_get_current_speed());
        //cJSON_AddStringToObject(motor_obj, "dir", forward ? "fwd" : "rev");
        cJSON_AddItemToObject(status_payload, "motor", motor_obj);
    }

    cJSON_AddBoolToObject(status_payload, "camera_active", camera_manager_is_active());
    cJSON_AddBoolToObject(status_payload, "telemetry_active", telemetry_manager_is_active());

    ESP_LOGI(TAG, "Sending real-time train status to target: %s...", target);    
    send_ws_envelope(client, "train_status", target, status_payload);
}