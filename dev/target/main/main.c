#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "main_config.h"

void app_main(void) {
    // 1. Initialisation système de base
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();

    // 2. Lancement du service Wi-Fi
    // Le Wi-Fi lancera lui-même le WS et l'OTA une fois connecté
    ESP_LOGI(TAG, "Initialisation Wi-Fi...");
    wifi_init_sta();
}