#include "websocket_manager/websocket_manager.h"
#include "ota_manager/ota_manager.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include <string.h>
#include "esp_mac.h"
#include "cJSON.h"             
#include "freertos/FreeRTOS.h"  
#include "freertos/task.h"
#include "esp_timer.h"
#include "turnout_manager/turnout_manager.h"

static const char *TAG = "websocket_mgr";
static esp_websocket_client_handle_t client = NULL;
static TaskHandle_t heartbeat_task_handle = NULL;

#define WEBSOCKET_URI "ws://192.168.10.1:8080/ws/%s?type=track_controller" 

void get_mac_address(char *out_str, size_t size);
static char* get_ws_identifier(void);
void send_ws_envelope(esp_websocket_client_handle_t client, const char* type, const char* target, cJSON* payload);
static void websocket_heartbeat_task(void *pvParameters);
void websocket_stop_heartbeat(void);
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void process_incoming_message(const char *data, size_t len);
static void handle_system_command(cJSON *payload);
static cJSON* create_system_info_payload(void);
static void send_get_turnouts(const char *target_device);
static bool handle_turnout_control_command(cJSON *payload);

void get_mac_address(char *out_str, size_t size) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out_str, size, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static char* get_ws_identifier() {
    static char mac_id[13];
    get_mac_address(mac_id, sizeof(mac_id));
    return mac_id;
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_client_handle_t client_h = (esp_websocket_client_handle_t)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connecté au serveur WebSocket !");
            
            if (heartbeat_task_handle == NULL) {
                xTaskCreate(websocket_heartbeat_task, "ws_heartbeat", 3072, client_h, 3, &heartbeat_task_handle);
            }
            break;
            
        case WEBSOCKET_EVENT_DISCONNECTED:
            websocket_stop_heartbeat();
            ESP_LOGW(TAG, "Déconnecté du serveur WebSocket. Tentative de reconnexion automatique en cours...");
            break;
            
        case WEBSOCKET_EVENT_DATA:
            if (data->data_len > 0 && data->op_code == 0x01) {
                process_incoming_message((char *)data->data_ptr, data->data_len);
            }
            break;
            
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            break;
    }
}

void websocket_manager_init(void)
{
    static char full_url[128];
    snprintf(full_url, sizeof(full_url), WEBSOCKET_URI, get_ws_identifier());

    ESP_LOGI(TAG, "Connecting to %s ...", full_url);
    esp_websocket_client_config_t websocket_cfg = {
        .uri = full_url,
    };

    ESP_LOGI(TAG, "Initialisation du client WebSocket sur %s ...", full_url);
    client = esp_websocket_client_init(&websocket_cfg);
    
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    esp_websocket_client_start(client);
}

void send_ws_envelope(esp_websocket_client_handle_t client_handle, const char* type, const char* target, cJSON* payload) {
    if (client_handle == NULL || !esp_websocket_client_is_connected(client_handle)) {
        if (payload) cJSON_Delete(payload); // Évite les fuites de mémoire si non connecté
        return;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        if (payload) cJSON_Delete(payload);
        return;
    }

    cJSON_AddStringToObject(root, "type", type ? type : "");
    cJSON_AddStringToObject(root, "source", get_ws_identifier()); 
    cJSON_AddStringToObject(root, "target", target ? target : "server");
    cJSON_AddNumberToObject(root, "timestamp", (long)(esp_timer_get_time() / 1000000));
    
    if (payload) {
        cJSON_AddItemToObject(root, "payload", payload);
    } else {
        cJSON_AddNullToObject(root, "payload"); 
    }

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        esp_websocket_client_send_text(client_handle, json_str, strlen(json_str), pdMS_TO_TICKS(1000));
        cJSON_free(json_str); 
    }
    
    cJSON_Delete(root); 
}

static void websocket_heartbeat_task(void *pvParameters) {
    esp_websocket_client_handle_t client_h = (esp_websocket_client_handle_t)pvParameters;
    while (1) {
        if (esp_websocket_client_is_connected(client_h)) {
            send_ws_envelope(client_h, "ping", "server", NULL);
            ESP_LOGD(TAG, "Heartbeat sent");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void websocket_stop_heartbeat() {
    if (heartbeat_task_handle != NULL) {
        vTaskDelete(heartbeat_task_handle);
        heartbeat_task_handle = NULL;
        ESP_LOGI(TAG, "Heartbeat task stopped");
    }
}

static void process_incoming_message(const char *data, size_t len) {
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (root == NULL) return;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    cJSON *source = cJSON_GetObjectItem(root, "source");
    const char *source_str = (cJSON_IsString(source)) ? source->valuestring : "unknown";

    if (cJSON_IsString(type)) {
        const char *type_str = type->valuestring;

        if (strcmp(type_str, "welcom") == 0) {
            ESP_LOGI(TAG, "Server authentication successful");
        }
        else if (strcmp(type_str, "system") == 0) {
            handle_system_command(payload);
        }
        else if (strcmp(type_str, "set_turnouts") == 0) {
            if (handle_turnout_control_command(payload)) {
                send_get_turnouts(source_str);
            }
        }
        else if (strcmp(type_str, "get_turnouts") == 0) {
            ESP_LOGI(TAG, "Request received: Turnout status");
            send_get_turnouts(source_str);
        }
        else if (strcmp(type_str, "system_status") == 0) {
            ESP_LOGI(TAG, "Request received: General system info");
            if (client && esp_websocket_client_is_connected(client)) {
                cJSON *caps = create_system_info_payload();
                send_ws_envelope(client, "system_info_reply", "server", caps);
            }
        }
    }
    cJSON_Delete(root);
}

static void handle_system_command(cJSON *payload) {
    if (!cJSON_IsObject(payload)) return;

    cJSON *action_item = cJSON_GetObjectItem(payload, "action");
    if (!cJSON_IsString(action_item)) return;

    if (strcmp(action_item->valuestring, "update_firmware") == 0) {
        cJSON *src_item = cJSON_GetObjectItem(payload, "src");
        if (cJSON_IsString(src_item)) {
            ESP_LOGW(TAG, "Starting OTA update from: %s", src_item->valuestring);
            ota_manager_trigger_update(src_item->valuestring);
        }
    } 
    else if (strcmp(action_item->valuestring, "reboot") == 0) {
        ESP_LOGW(TAG, "Reboot command received. Restarting...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
}

/**
 * @brief Génère le payload d'information globale (partagé entre la connexion et la réponse à la demande)
 */
static cJSON* create_system_info_payload(void) {
    cJSON *caps = cJSON_CreateObject();
    if (!caps) return NULL;

    cJSON_AddStringToObject(caps, "device", get_ws_identifier());
    cJSON *switchers_array = cJSON_CreateArray();

    int count = turnout_manager_get_count();
    for (int i = 0; i < count; i++) {
        cJSON *switcher = cJSON_CreateObject();
        cJSON_AddNumberToObject(switcher, "id", i);
        
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "Turnout %d", i);
        cJSON_AddStringToObject(switcher, "name", name_buf);

        turnout_position_t position = turnout_manager_get_position(i);
        if (position == TURNOUT_POSITION_DIVERGENT) {
            cJSON_AddStringToObject(switcher, "position", "divergent");
        } else {
            cJSON_AddStringToObject(switcher, "position", "normal");
        }

        cJSON_AddItemToArray(switchers_array, switcher);
    }

    cJSON_AddItemToObject(caps, "active_switchers", switchers_array);
    return caps;
}

/**
 * @brief Envoie l'état actuel des turnouts de façon compacte (réponse à turnout_status_request)
 */
static void send_get_turnouts(const char *target_device) {
    if (client == NULL || !esp_websocket_client_is_connected(client)) return;

    cJSON *payload = cJSON_CreateObject();
    cJSON *status_array = cJSON_CreateArray();

    int count = turnout_manager_get_count();
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", i);
        
        turnout_position_t position = turnout_manager_get_position(i);
        cJSON_AddStringToObject(item, "position", (position == TURNOUT_POSITION_DIVERGENT) ? "divergent" : "normal");
        
        cJSON_AddItemToArray(status_array, item);
    }

    cJSON_AddItemToObject(payload, "turnouts", status_array);
    // Envoi sous le type "turnout_status_reply"
    send_ws_envelope(client, "get_turnouts", target_device, payload);
}

static bool handle_turnout_control_command(cJSON *payload)
{
    if (!payload) return false;

    cJSON *id_item = cJSON_GetObjectItem(payload, "id");
    cJSON *position_item = cJSON_GetObjectItem(payload, "position");

    if (!cJSON_IsNumber(id_item) || !cJSON_IsString(position_item)) {
        ESP_LOGE(TAG, "Format JSON invalide reçu pour l'aiguillage.");
        return false;
    }

    int target_id = id_item->valueint;
    const char *position_str = position_item->valuestring;
    turnout_position_t target_position;

    if (strcmp(position_str, "normal") == 0) {
        target_position = TURNOUT_POSITION_NORMAL;
    } else if (strcmp(position_str, "divergent") == 0) {
        target_position = TURNOUT_POSITION_DIVERGENT;
    } else {
        ESP_LOGW(TAG, "Chaîne d'état inconnue dans le JSON: '%s'", position_str);
        return false;
    }

    return turnout_manager_set_position(target_id, target_position);
    
}