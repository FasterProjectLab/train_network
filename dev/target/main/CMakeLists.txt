set(srcs 
    "main.c" 
    "wifi_service.c" 
    "ws_service.c" 
    "ota_service.c"
    "camera_service.c"
    "telemetry_service.c"
    "motor_service/motor_service.c"
    "light_service.c"
    "i2c_service/i2c_service.c"
    "nfc_service/nfc_service.c")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    EMBED_TXTFILES "./server_cert.pem"
    # Liste exhaustive des dépendances nécessaires :
    REQUIRES 
        esp_driver_i2c
        esp32-camera
        esp_http_server
    PRIV_REQUIRES 
        nvs_flash           # Pour nvs_flash.h
        esp_wifi            # Pour esp_wifi.h
        esp_event           # Pour les boucles d'événements
        esp_netif           # Pour la pile réseau
        esp_websocket_client # Pour le WebSocket
        esp_http_client     # Requis par l'OTA
        esp_https_ota       # Pour l'OTA
        json                # Pour cJSON.h
        esp_partition       # Pour la gestion des partitions
        app_update          # Pour la validation du nouveau firmware
)