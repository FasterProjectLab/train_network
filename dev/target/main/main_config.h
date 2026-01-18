#ifndef MAIN_CONFIG_H
#define MAIN_CONFIG_H

#include "esp_log.h"

#define WIFI_SSID      "train_network"
#define WIFI_PASS      "password123"
#define SERVER_WS_URL  "ws://192.168.10.1:8080/ws/train-%s"
#define FIRMWARE_URL   "https://192.168.10.1/firmware.bin"

static const char *TAG = "ESP32_APP";

// Prototypes pour lier les fichiers
void wifi_init_sta(void);
void websocket_app_start(void);
void start_ota_update(void);

#endif