set(srcs 
    "main.c" 
    "wifi_manager/wifi_manager.c"
    "ota_manager/ota_manager.c"
    "websocket_manager/websocket_manager.c"
    "turnout_manager/turnout_manager.c")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    EMBED_TXTFILES "./server_cert.pem"
    # Liste exhaustive des dépendances nécessaires :
    PRIV_REQUIRES 
        nvs_flash           # Pour nvs_flash.h
        esp_wifi            # Pour esp_wifi.h
        esp_event           # Pour les boucles d'événements
        esp_netif           # Pour la pile réseau
        esp_websocket_client # Pour le WebSocket
        esp_http_client     # Requis par l'OTA
        esp_https_ota       # Pour l'OTA
        cjson                # Pour cJSON.h
        esp_partition       # Pour la gestion des partitions
        app_update          # Pour la validation du nouveau firmware
        esp_http_server
        esp_driver_ledc
)