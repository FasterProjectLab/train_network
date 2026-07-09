#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager/wifi_manager.h" 
#include "ota_manager/ota_manager.h" 
#include "websocket_manager/websocket_manager.h"
#include "turnout_manager/turnout_manager.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Démarrage des services...");

    wifi_manager_init();
    turnout_manager_init();

    websocket_manager_init();

    // 4. Boucle principale de votre application métier
    while (1) {
        ESP_LOGI(TAG, "Application principale active [Wi-Fi en ligne : %s]", 
                 wifi_manager_is_connected() ? "OUI" : "NON");
                 
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}