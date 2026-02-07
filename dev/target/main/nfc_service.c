#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "main_config.h"

#define PN532_ADDR 0x24

esp_err_t nfc_send_command(uint8_t *cmd, size_t cmd_len) {
    uint8_t packet[cmd_len + 8];
    uint8_t len = cmd_len + 1;
    uint8_t lcs = (uint8_t)(~len + 1);
    
    packet[0] = 0x00; packet[1] = 0x00; packet[2] = 0xFF;
    packet[3] = len; packet[4] = lcs; packet[5] = 0xD4;

    uint8_t dcs_sum = 0xD4;
    for (size_t i = 0; i < cmd_len; i++) {
        packet[6 + i] = cmd[i];
        dcs_sum += cmd[i];
    }
    packet[6 + cmd_len] = (uint8_t)(~dcs_sum + 1);
    packet[7 + cmd_len] = 0x00;

    return i2c_service_write(PN532_ADDR, packet, sizeof(packet));
}

esp_err_t nfc_read_tag_uid(uint8_t *uid_out, uint8_t *uid_len) {
    uint8_t buffer[64] = {0};
    
    // 1. Première lecture pour voir si le module a quelque chose
    if (i2c_service_receive(PN532_ADDR, buffer, sizeof(buffer)) != ESP_OK) return ESP_FAIL;

    // 2. Si on reçoit un ACK (00 00 FF 00 FF 00), on doit lire à nouveau pour la DATA
    if (buffer[1] == 0x00 && buffer[2] == 0x00 && buffer[3] == 0xFF) {
        // Laisser le temps au scan RF de finir (important pour ISO14443-3A)
        vTaskDelay(pdMS_TO_TICKS(30)); 
        if (i2c_service_receive(PN532_ADDR, buffer, sizeof(buffer)) != ESP_OK) return ESP_FAIL;
    }

    // 3. Vérifier le Ready Bit (0x01)
    if (buffer[0] != 0x01) return ESP_ERR_NOT_FINISHED;

    // 4. Chercher la signature de réponse InListPassiveTarget (D5 4B)
    for (int i = 1; i < sizeof(buffer) - 10; i++) {
        if (buffer[i] == 0xD5 && buffer[i+1] == 0x4B) {
            uint8_t nb_tags = buffer[i+2];
            if (nb_tags > 0) {
                *uid_len = buffer[i+6]; // Offset de l'UID dans la trame 0x4B
                memcpy(uid_out, &buffer[i+7], *uid_len);
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}

void nfc_polling_task(void *pvParameters) {
    uint8_t buffer[64];
    uint8_t ack_buffer[12]; // 12 octets suffisent largement pour un ACK
    
    // Commande SAMConfig (pour réveiller la puce)
    uint8_t sam_cmd[] = { 0x14, 0x01, 0x14, 0x01 };
    nfc_send_command(sam_cmd, 4);

    // Délai très court : le PN532 génère l'ACK presque instantanément
    vTaskDelay(pdMS_TO_TICKS(20)); 

    // Lecture brute
    memset(ack_buffer, 0, sizeof(ack_buffer));
    if (i2c_service_receive(PN532_ADDR, ack_buffer, sizeof(ack_buffer)) == ESP_OK) {
        // ON VEUT VOIR : 01 00 00 FF 00 FF 00
        ESP_LOG_BUFFER_HEX("NFC_RAW_ACK", ack_buffer, sizeof(ack_buffer));
    } else {
        ESP_LOGE("NFC_STEP", "Erreur de lecture I2C");
    }

    while (1) {
        ESP_LOGI("NFC_STEP", "Envoi commande...");
        
        uint8_t detect_cmd[] = { 0x4A, 0x01, 0x00 };
        ESP_LOGI("NFC_STEP", "Scan du tag...");
        nfc_send_command(detect_cmd, 3);

        vTaskDelay(pdMS_TO_TICKS(150));

        memset(buffer, 0, sizeof(buffer));

        if (i2c_service_receive(PN532_ADDR, buffer, sizeof(buffer)) == ESP_OK) {

            ESP_LOG_BUFFER_HEX("I2C_RX_RAW", buffer, 32);
            
            // On cherche la signature D5 4B dans toute la trame
            bool tag_found = false;
            for (int i = 0; i < sizeof(buffer) - 5; i++) {
                if (buffer[i] == 0xD5 && buffer[i+1] == 0x4B) {
                    tag_found = true;
                    uint8_t uid_len = buffer[i+6];
                    ESP_LOGI("NFC_SUCCESS", "TAG DETECTÉ ! UID Longueur: %d", uid_len);
                    esp_log_buffer_hex("NFC_UID_VAL", &buffer[i+7], uid_len);
                    break;
                }
            }

            if (!tag_found) {
                // Si on ne voit que l'ACK (00 00 FF 00 FF 00), c'est qu'aucun tag n'est présent
                ESP_LOGD("NFC_STEP", "Rien sur l'antenne...");
            } else {
                // Si on a trouvé un tag, on attend un peu plus avant le prochain scan
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        

        ESP_LOGI("NFC_STEP", "end");
        vTaskDelay(pdMS_TO_TICKS(2000)); // On recommence toutes les 2 secondes
    }
}

void nfc_service_init(void) {
    xTaskCreate(nfc_polling_task, "nfc_task", 4096, NULL, 5, NULL);
}