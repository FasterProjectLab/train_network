#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "main_config.h"

/**
 * @brief Application Entry Point
 * Coordinates the initialization of non-volatile storage, network interfaces,
 * hardware peripherals (Camera, Lights, Motors), and Wi-Fi connectivity.
 */
void app_main(void) {
    ESP_LOGI(TAG, "Starting Application Main...");

    // 1. Initialize Non-Volatile Storage (NVS)
    // Required by the Wi-Fi stack to store configuration and calibration data
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated or format changed. Erasing and retrying...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Core System Components
    // Initialize the underlying TCP/IP stack and the default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Hardware Service Initializations
    // Priority 1: Camera (Critical peripheral)
    if (camera_init_service() != ESP_OK) {
        ESP_LOGE(TAG, "Critical Camera initialization failed. Halting system.");
        return;
    }

    // Priority 2: Peripherals (Lights and Motors)
    ESP_LOGI(TAG, "Initializing hardware peripherals...");
    light_service_init();
    motor_service_init();

    // 4. Network Connectivity
    /* * Start Wi-Fi in Station mode.
     * Note: This service is asynchronous. Once a connection is established, 
     * its internal event handler will automatically trigger the WebSocket 
     * and OTA services.
     */
    ESP_LOGI(TAG, "Starting Wi-Fi Station initialization...");
    wifi_init_sta();

    ESP_LOGI(TAG, "System initialization sequence complete. Event loop running.");
}