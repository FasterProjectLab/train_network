#include "ota_manager/ota_manager.h"
#include "wifi_manager/wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_system.h"

static const char *TAG = "ota_manager";

// Reference to the embedded certificate for HTTPS validation
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");

static void ota_download_task(void *pvParameter)
{
    char *url_to_use = (char *)pvParameter;

    if (url_to_use == NULL) {
        ESP_LOGE(TAG, "Invalid URL (NULL) provided to OTA task");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Début du téléchargement du firmware OTA...");

    if (!wifi_manager_is_connected()) {
        ESP_LOGE(TAG, "Annulation OTA : Le Wi-Fi s'est déconnecté entre-temps.");
        vTaskDelete(NULL);
        return;
    }

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

    // Exécution de l'OTA (bloquant pendant le téléchargement)
    esp_err_t ret = esp_https_ota(&ota_config);
    free(url_to_use);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Mise à jour OTA réussie ! Redémarrage de l'ESP32 dans 2 secondes...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Échec de la mise à jour OTA (Code: %s).", esp_err_to_name(ret));
    }

    // Si l'OTA a échoué, on détruit cette tâche pour libérer la mémoire RAM de l'ESP32
    ESP_LOGI(TAG, "Libération de la tâche OTA.");
    vTaskDelete(NULL);
}

void ota_manager_trigger_update(const char* url)
{
    char* url_copy = strdup(url);
    
    if (url_copy == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed for OTA URL copy");
        return;
    }

    ESP_LOGW(TAG, "Ordre de mise à jour reçu via WebSocket.");

    if (!wifi_manager_is_connected()) {
        ESP_LOGE(TAG, "Impossible de lancer l'OTA : Pas de connexion Wi-Fi.");
        return;
    }

    // Création de la tâche de téléchargement (Priorité 6 pour être au-dessus du WebSocket)
    BaseType_t xReturned = xTaskCreate(&ota_download_task, "ota_download_task", 8192, (void*)url_copy, 6, NULL);

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        free(url_copy); // Clean up if task creation failed
    }
}