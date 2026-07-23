#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "main_config.h"

static const char *TAG = "WIFI_MANAGER";

/**
 * @brief Event handler for Wi-Fi and IP events
 * * Handles connection logic, automatic reconnection, and triggers
 * network-dependent services once an IP address is assigned.
 */
static void event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        // Initial start: attempt to connect to the configured AP
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        // Connection lost: trigger immediate reconnection logic
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying connection...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        // IP address acquired
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) data;
        ESP_LOGI(TAG, "Connected! IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        
        /* * Disable Wi-Fi Power Save Mode for better performance.
         * Crucial for stable WebSocket/Low-latency applications.
         */
        esp_wifi_set_ps(WIFI_PS_NONE);

        // Start higher-level network services
        websocket_app_start();
    }
}

/**
 * @brief Initializes the Wi-Fi Station (STA) mode
 * * Sets up the network interface, event loop handlers, and credentials.
 */
void wifi_init_sta(void) {
    // 1. Initialize the underlying TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // 2. Create default network interface instance for Station
    esp_netif_create_default_wifi_sta();

    // 3. Initialize Wi-Fi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. Register event handlers for Wi-Fi and IP stack events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, 
                                                        ESP_EVENT_ANY_ID, 
                                                        &event_handler, 
                                                        NULL, 
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, 
                                                        IP_EVENT_STA_GOT_IP, 
                                                        &event_handler, 
                                                        NULL, 
                                                        NULL));

    // 5. Configure Wi-Fi Station settings
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Setting threshold to WPA2 allows connection to most modern routers */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 6. Start the Wi-Fi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi Station initialization finished.");
}