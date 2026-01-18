set(srcs 
    "main.c" 
    "wifi_service.c" 
    "ws_service.c" 
    "ota_service.c")

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
        json                # Pour cJSON.h
        esp_partition       # Pour la gestion des partitions
        app_update          # Pour la validation du nouveau firmware
)