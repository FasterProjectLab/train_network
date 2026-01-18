#include "esp_https_ota.h"
#include "main_config.h"

extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");

static void ota_task(void *pvParameter) {
    ESP_LOGI(TAG, "Vérification mise à jour OTA...");
    esp_http_client_config_t config = {
        .url = FIRMWARE_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .skip_cert_common_name_check = true,
    };
    esp_https_ota_config_t ota_config = { .http_config = &config };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Success. Rebooting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA Failed: %s", esp_err_to_name(ret));
    }
    vTaskDelete(NULL);
}

void start_ota_update(void) {
    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}