#ifndef MAIN_CONFIG_H
#define MAIN_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_websocket_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdbool.h>

// ===== NETWORK CONFIGURATION =====
#define WIFI_SSID      "train_network"
#define WIFI_PASS      "password123"
/** * @brief WebSocket Server URL template. 
 * %s will be replaced by the device MAC address. 
 */
#define SERVER_WS_URL  "ws://192.168.10.1:8080/ws/%s?type=train"

/** @brief Global logging tag for the application */
static const char *TAG = "ESP32_TRAIN_APP";

// ===== SYSTEM & OTA =====
/** @brief Starts the Wi-Fi station mode */
void wifi_init_sta(void);


// ===== WEBSOCKET APPLICATION =====
/** @brief Starts the WebSocket client task */
void websocket_app_start(void);

/** * @brief Sends a formatted JSON message over WebSocket
 * @param payload The cJSON object to send (function takes ownership and will delete it)
 */
void send_ws_envelope(esp_websocket_client_handle_t client, const char* type, const char* target, cJSON* payload);

/** * @brief Automatically updates lights based on the motor's current direction.
 * * Switches between front and rear lighting sets (e.g., white at the front, 
 * red at the rear) depending on whether the motor is in forward or reverse.
 */


#ifdef __cplusplus
}
#endif

#endif // MAIN_CONFIG_H