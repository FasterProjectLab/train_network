#include "esp_websocket_client.h"
#include "main_config.h"
#include <string.h>
#include "cJSON.h"
#include <stdio.h>
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_camera.h"

static esp_timer_handle_t heartbeat_timer;

#include "esp_websocket_client.h"

// Instance du client WebSocket
esp_websocket_client_handle_t client_v;

static void websocket_event_handler_v(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            printf("WebSocket: Connecté au serveur\n");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            printf("WebSocket: Déconnecté\n");
            break;
        case WEBSOCKET_EVENT_DATA:
            // Si le serveur envoie une réponse, elle arrive ici
            break;
        case WEBSOCKET_EVENT_ERROR:
            printf("WebSocket: Erreur\n");
            break;
    }
}

void init_websocket_v() {
    esp_websocket_client_config_t ws_cfg = {
        .uri = "ws://192.168.10.1:8090", // REMPLACEZ PAR L'IP DE VOTRE PC
    };

    client_v = esp_websocket_client_init(&ws_cfg);
    
    // Enregistrement du gestionnaire d'événements
    esp_websocket_register_events(client_v, WEBSOCKET_EVENT_ANY, websocket_event_handler_v, (void *)client_v);

    // Démarrage du client
    esp_websocket_client_start(client_v);
}

void stream_task(void *pvParameters) {
    while (1) {
        camera_fb_t * fb = esp_camera_fb_get();
        if (fb) {
            if (esp_websocket_client_is_connected(client_v)) {
                // Envoi du JPEG brut au serveur Python
                esp_websocket_client_send_bin(client_v, (const char *)fb->buf, fb->len, portMAX_DELAY);
            }
            esp_camera_fb_return(fb);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // ~20 FPS maximum
    }
}

static void send_heartbeat_ping(void* arg) {
    esp_websocket_client_handle_t client_handle = (esp_websocket_client_handle_t)arg;
    
    if (esp_websocket_client_is_connected(client_handle)) {
        const char *ping_msg = "{\"type\": \"ping\"}";
        esp_websocket_client_send_text(client_handle, ping_msg, strlen(ping_msg), portMAX_DELAY);
        ESP_LOGD(TAG, "Heartbeat envoyé");
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_client_handle_t client = (esp_websocket_client_handle_t)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WS: Connecté");
            const esp_timer_create_args_t timer_args = {
                .callback = &send_heartbeat_ping,
                .arg = (void*)client,
                .name = "heartbeat"
            };
            esp_timer_create(&timer_args, &heartbeat_timer);
            esp_timer_start_periodic(heartbeat_timer, 5 * 1000000);


        case WEBSOCKET_EVENT_DATA:
            if (data->data_len > 0 && data->op_code == 0x01) {
                ESP_LOGI(TAG, "WS: Données reçues (%d octets): %.*s", data->data_len, data->data_len, (char *)data->data_ptr);

                // 1. Parser le message reçu
                cJSON *root = cJSON_ParseWithLength((char *)data->data_ptr, data->data_len);
                if (root == NULL) {
                    const char *error_ptr = cJSON_GetErrorPtr();
                    if (error_ptr != NULL) {
                        ESP_LOGE(TAG, "Erreur JSON avant : %s", error_ptr);
                    }
                    break; 
                }

                // 2. Extraire les champs
                cJSON *type = cJSON_GetObjectItem(root, "type");
                cJSON *content = cJSON_GetObjectItem(root, "data");

                // 3. Logique de décision
                if (cJSON_IsString(type) && strcmp(type->valuestring, "push") == 0) {
                    if (cJSON_IsString(content) && strcmp(content->valuestring, "update firmware") == 0) {
                        ESP_LOGW(TAG, "ORDRE OTA REÇU ! Lancement du service...");
                        
                        // Appel de la fonction définie dans ota_service.c
                        start_ota_update();
                    } else if (cJSON_IsString(content) && strcmp(content->valuestring, "start camera") == 0) {
                        

                    } else if (cJSON_IsString(content) && strcmp(content->valuestring, "stop camera") == 0) {
                        ESP_LOGI(TAG, "Arrêt du flux vidéo");
                    }
                }

                cJSON_Delete(root); // Toujours libérer la mémoire cJSON
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WS: Déconnecté");

            esp_timer_stop(heartbeat_timer);
            esp_timer_delete(heartbeat_timer);
            break;
    }
}

void get_mac_address(char *out_str, size_t size) {
    uint8_t mac[6];
    
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    snprintf(out_str, size, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void websocket_app_start(void) {
    static char full_url[128];
    char mac_id[13];

    // get_mac_address(mac_id, sizeof(mac_id));
    // snprintf(full_url, sizeof(full_url), SERVER_WS_URL, mac_id);

    // const esp_websocket_client_config_t ws_cfg = {
    //     .uri = full_url,
    //     .buffer_size = 10240, // Augmentez à 10 Ko ou plus
    //     .reconnect_timeout_ms = 10000,
    //     .network_timeout_ms = 10000,
    // };

    // ESP_LOGI(TAG, "Connexion à : %s", full_url);

    // esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_cfg);
    // esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    // esp_websocket_client_start(client);

    init_websocket_v();

    xTaskCreatePinnedToCore(
        stream_task,    // Nom de la fonction
        "stream_task",  // Nom pour le debug
        4096,           // Taille de la pile (Stack size en octets)
        NULL,           // Paramètres à passer
        5,              // Priorité (5 est une priorité moyenne/haute)
        NULL,            // Handle de la tâche (optionnel)
        1
    );
}

