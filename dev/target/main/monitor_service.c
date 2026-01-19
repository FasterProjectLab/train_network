#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

static const char *TAG = "PERF_SERVICE";
static esp_timer_handle_t monitor_timer = NULL;

/**
 * Calcule la charge CPU globale simplifiée.
 * Note: vTaskGetRunTimeStats est plus précis mais nécessite une config spécifique.
 */
static void monitoring_callback(void* arg) {
    uint32_t free_ram = esp_get_free_heap_size();
    uint32_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    // Header
    printf("\n\033[1;36m--- ESP32-S3 SYSTEM MONITOR ---\033[0m\n");
    
    // RAM & PSRAM
    printf("RAM Libre    : %lu octets\n", (unsigned long)free_ram);
    printf("PSRAM Libre  : %u octets\n", (unsigned int)free_psram);

    // Charge CPU (Via RunTimeStats si activé dans menuconfig)
    #if configGENERATE_RUN_TIME_STATS
        char buf[1024];
        vTaskGetRunTimeStats(buf);
        printf("CPU Load Detail:\n%s", buf);
    #else
        // Alternative simple : Affichage de la stack minimale restante pour la tâche actuelle
        printf("CPU Info     : Activez 'configGENERATE_RUN_TIME_STATS' pour le %% CPU\n");
        printf("Min Stack    : %u bytes\n", (unsigned int)uxTaskGetStackHighWaterMark(NULL));
    #endif

    printf("\033[1;36m--------------------------------\033[0m\n");
}

esp_err_t perf_monitor_start(uint32_t interval_ms) {
    if (monitor_timer != NULL) return ESP_OK;

    const esp_timer_create_args_t timer_args = {
        .callback = &monitoring_callback,
        .name = "perf_monitor_timer"
    };

    esp_err_t err = esp_timer_create(&timer_args, &monitor_timer);
    if (err == ESP_OK) {
        // Correction ici : suppression de l'espace dans le formatage
        err = esp_timer_start_periodic(monitor_timer, (uint64_t)interval_ms * 1000);
        ESP_LOGI(TAG, "Service de monitoring démarré (%lums)", (unsigned long)interval_ms);
    }
    return err;
}

void perf_monitor_stop() {
    if (monitor_timer != NULL) {
        esp_timer_stop(monitor_timer);
        esp_timer_delete(monitor_timer);
        monitor_timer = NULL;
        ESP_LOGI(TAG, "Service de monitoring arrêté");
    }
}