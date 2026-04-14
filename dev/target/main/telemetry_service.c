#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "main_config.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "driver/temperature_sensor.h" 

// Static handles and variables
static esp_timer_handle_t telemetry_timer = NULL;
static esp_websocket_client_handle_t ws_client_handle = NULL;
static temperature_sensor_handle_t temp_sensor = NULL;
static uint32_t task_interval_ms = 5000;
static bool telemetry_state = false;

// Forward declarations
void telemetry_start();
void telemetry_stop();

/**
 * @brief Initialize the internal ESP32-S3 temperature sensor
 */
static void init_temp_sensor() {
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) == ESP_OK) {
        temperature_sensor_enable(temp_sensor);
        ESP_LOGI(TAG, "Temperature sensor initialized");
    } else {
        ESP_LOGE(TAG, "Failed to install temperature sensor");
    }
}

typedef struct {
    uint32_t core0;
    uint32_t core1;
} cpu_usage_t;

/**
 * @brief Calculates CPU usage per core based on Idle task run time
 * Requires CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS to be enabled in menuconfig
 */
static cpu_usage_t get_cpu_usage_per_core() {
    static uint32_t last_idle0_time = 0, last_idle1_time = 0;
    static uint32_t last_total_time = 0;
    cpu_usage_t usage = {0, 0};

#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    TaskStatus_t *pxTaskStatusArray;
    uint32_t ulTotalRunTime;
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();

    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    if (pxTaskStatusArray != NULL) {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);
        
        // Calculate difference only if total runtime has increased
        if (ulTotalRunTime > last_total_time) {
            uint32_t current_idle0 = 0, current_idle1 = 0;

            for (int i = 0; i < uxArraySize; i++) {
                if (strcmp(pxTaskStatusArray[i].pcTaskName, "IDLE0") == 0) {
                    current_idle0 = pxTaskStatusArray[i].ulRunTimeCounter;
                } else if (strcmp(pxTaskStatusArray[i].pcTaskName, "IDLE1") == 0) {
                    current_idle1 = pxTaskStatusArray[i].ulRunTimeCounter;
                }
            }

            uint32_t diff_total = ulTotalRunTime - last_total_time;

            // Core 0 calculation (Usage = 100% - Idle%)
            if (current_idle0 >= last_idle0_time) {
                uint32_t diff_idle0 = current_idle0 - last_idle0_time;
                usage.core0 = 100 - ((100 * diff_idle0) / diff_total);
            }

            // Core 1 calculation
            if (current_idle1 >= last_idle1_time) {
                uint32_t diff_idle1 = current_idle1 - last_idle1_time;
                usage.core1 = 100 - ((100 * diff_idle1) / diff_total);
            }

            // Store values for the next cycle
            last_idle0_time = current_idle0;
            last_idle1_time = current_idle1;
            last_total_time = ulTotalRunTime;
        }
        vPortFree(pxTaskStatusArray);
    }
#endif
    return usage;
}

/**
 * @brief Timer callback that collects system metrics and sends them via WebSocket
 */
static void telemetry_callback(void* arg) {
    if (ws_client_handle == NULL || !esp_websocket_client_is_connected(ws_client_handle)) {
        return;
    }

    ESP_LOGD(TAG, "Collecting telemetry data...");

    // 1. Collect CPU metrics (Dual core)
    cpu_usage_t cpu = get_cpu_usage_per_core();

    // 2. Collect Memory metrics (Internal & External RAM)
    uint32_t free_ram = esp_get_free_heap_size();
    uint32_t min_ram = esp_get_minimum_free_heap_size();
    uint32_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t stack_free_bytes = uxTaskGetStackHighWaterMark(NULL);

    // 3. Collect WiFi metrics (Signal strength)
    int rssi = -127;
    wifi_ap_record_t ap;
    char* wifi_std = "unknown";
    uint8_t primary_chan = 0;
    wifi_second_chan_t second_chan= WIFI_SECOND_CHAN_NONE; 
    wifi_bandwidth_t bw_type = WIFI_BW_HT20;
    int bw_real = 20;

    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        rssi = ap.rssi;

        if (ap.phy_11n) {
            wifi_std = "802.11n";
        }
        else if (ap.phy_11g) wifi_std = "802.11g";
        else if (ap.phy_11b) wifi_std = "802.11b";

        esp_wifi_get_channel(&primary_chan, &second_chan);

        esp_wifi_get_bandwidth(WIFI_IF_STA, &bw_type);

        if (bw_type == WIFI_BW_HT40) {
            if (second_chan == WIFI_SECOND_CHAN_ABOVE) bw_real = +40;      
            else if (second_chan == WIFI_SECOND_CHAN_BELOW) bw_real = -40; 
        }
    }

    // 4. Collect Internal Temperature
    float tsens_out = 0;
    if (temp_sensor) {
        temperature_sensor_get_celsius(temp_sensor, &tsens_out);
    }

    // 5. Build JSON Payload
    cJSON *payload = cJSON_CreateObject();
    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to create telemetry JSON object");
        return;
    }
    
    cJSON_AddNumberToObject(payload, "cpu0", cpu.core0);
    cJSON_AddNumberToObject(payload, "cpu1", cpu.core1);
    cJSON_AddNumberToObject(payload, "temp", (int)tsens_out);
    cJSON_AddNumberToObject(payload, "ram_free", free_ram);
    cJSON_AddNumberToObject(payload, "ram_min", min_ram);
    cJSON_AddNumberToObject(payload, "psram_free", free_psram);

    cJSON_AddNumberToObject(payload, "rssi", rssi);
    cJSON_AddStringToObject(payload, "wifi_std", wifi_std);
    cJSON_AddNumberToObject(payload, "wifi_chan", primary_chan);
    cJSON_AddNumberToObject(payload, "wifi_bw_able", (bw_type == WIFI_BW_HT40) ? 40 : 20);
    cJSON_AddNumberToObject(payload, "wifi_bw_real", bw_real); // 0: HT20, 1: HT40+, 2: HT40-

    cJSON_AddNumberToObject(payload, "uptime", (long)(esp_timer_get_time() / 1000000));
    
    // Include stack high water mark for debugging purposes
    cJSON_AddNumberToObject(payload, "stack_mon", (unsigned int)stack_free_bytes);

    // Send via WebSocket envelope
    ESP_LOGI(TAG, "Sending telemetry heartbeat");
    send_ws_envelope(ws_client_handle, "telemetry", "broadcast", payload);
    
    // Note: cJSON_Delete(payload) is usually handled inside send_ws_envelope 
    // depending on your implementation.
}

/**
 * @brief Initialize the telemetry timer and peripherals
 * @param interval_ms Frequency of telemetry updates
 * @param client WebSocket handle for data transmission
 */
esp_err_t telemetry_task_init(uint32_t interval_ms, esp_websocket_client_handle_t client) {
    ws_client_handle = client;
    
    if (telemetry_timer != NULL) return ESP_OK;

    init_temp_sensor();

    task_interval_ms = interval_ms;

    const esp_timer_create_args_t timer_args = {
        .callback = &telemetry_callback,
        .name = "telemetry_timer"
    };

    esp_err_t err = esp_timer_create(&timer_args, &telemetry_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create telemetry timer: %s", esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Enable or disable the telemetry periodic service
 */
void telemetry_set_enabled(bool state, cJSON* payload) {
    if (telemetry_state == state) return;

    int interval = 1000;
    telemetry_state = state;

    ESP_LOGI(TAG, "Telemetry state changed to: %s with interval: %d ms", state ? "ENABLED" : "DISABLED", interval);

    if (telemetry_state) {

        if (payload != NULL && cJSON_IsObject(payload)) {
            cJSON* item = cJSON_GetObjectItem(payload, "interval");
            if (cJSON_IsNumber(item)) {
                interval = item->valueint;
            }
        }

        if (interval < 500) {
            interval = 500;
            ESP_LOGW(TAG, "Interval too short, clamped to %d ms", interval);
        } else if (interval > 5000) {
            interval = 5000;
            ESP_LOGW(TAG, "Interval too long, clamped to %d ms", interval);
        }

        telemetry_start(interval);
    } else {
        telemetry_stop();
    }
}

void telemetry_start(int interval_ms) {
    if (telemetry_timer == NULL) return;
    
    esp_err_t err = esp_timer_start_periodic(telemetry_timer, (uint64_t)interval_ms * 1000);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Telemetry service started (Interval: %lu ms)", (unsigned long)interval_ms);
    }
}

void telemetry_stop() {
    if (telemetry_timer != NULL) {
        esp_timer_stop(telemetry_timer);
        ESP_LOGI(TAG, "Telemetry service stopped");
    }
}