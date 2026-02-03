#include "esp_https_ota.h"
#include "main_config.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

// Reference to the embedded certificate for HTTPS validation
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");

/**
 * @brief Background task that performs the Over-The-Air (OTA) update
 * @param pvParameter Pointer to the duplicated URL string (must be freed here)
 */
static void ota_task(void *pvParameter) {
    char *url_to_use = (char *)pvParameter;

    if (url_to_use == NULL) {
        ESP_LOGE(TAG, "Invalid URL (NULL) provided to OTA task");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Starting HTTPS OTA update from: %s", url_to_use);

    // HTTP client configuration for the OTA download
    esp_http_client_config_t config = {
        .url = url_to_use,
        .cert_pem = (char *)server_cert_pem_start,
        .skip_cert_common_name_check = true, // Set to false in production for better security
        .keep_alive_enable = true,
    };
    
    esp_https_ota_config_t ota_config = { 
        .http_config = &config 
    };

    // This is a blocking call that handles the whole update process
    esp_err_t ret = esp_https_ota(&ota_config);
    
    // Memory cleanup: free the URL copy created in start_ota_update
    free(url_to_use);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful. Rebooting system...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }

    // Task must be deleted manually if it reaches the end
    vTaskDelete(NULL);
}

/**
 * @brief Initiates the OTA update process
 * @param url The HTTPS URL where the new binary is hosted
 */
void start_ota_update(const char* url) {
    /* * IMPORTANT: We must duplicate the URL string. The original JSON payload 
     * will be deleted from memory before the FreeRTOS task has a chance to read it.
     */
    char* url_copy = strdup(url);
    
    if (url_copy == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed for OTA URL copy");
        return;
    }

    // Create the OTA task with sufficient stack size (8KB)
    BaseType_t xReturned = xTaskCreate(&ota_task, "ota_task", 8192, (void*)url_copy, 5, NULL);
    
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        free(url_copy); // Clean up if task creation failed
    }
}