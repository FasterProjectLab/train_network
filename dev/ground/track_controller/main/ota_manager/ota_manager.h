#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

/**
 * @brief Déclenche immédiatement une mise à jour OTA (appelé par le WebSocket).
 */
void ota_manager_trigger_update(const char* url);

#endif // OTA_MANAGER_H