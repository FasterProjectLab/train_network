#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "main_config.h"

#define PN532_ADDR 0x24

typedef void (*nfc_tag_callback_t)(uint8_t *uid, uint8_t len);

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

esp_err_t nfc_check_ack(void) {
    uint8_t ack_buf[7]; // Statut (1) + ACK (6)
    if (i2c_service_receive(PN532_ADDR, ack_buf, sizeof(ack_buf)) != ESP_OK) return ESP_FAIL;

    //ESP_LOGI("NFC_ACK", "Trame ACK reçue: %02x %02x %02x %02x %02x %02x %02x",
    //         ack_buf[0], ack_buf[1], ack_buf[2], ack_buf[3], ack_buf[4], ack_buf[5], ack_buf[6]);
    
    // Un ACK valide est toujours : 00 00 FF 00 FF 00
    const uint8_t expected_ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    if (memcmp(&ack_buf[1], expected_ack, 6) == 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        return ESP_OK;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_ERR_INVALID_RESPONSE;
}

esp_err_t nfc_sam_config(void) {
    uint8_t sam_cmd[] = { 0x14, 0x01, 0x14, 0x01 };
    ESP_LOGI("NFC", "Configuration du SAM...");
    
    esp_err_t ret = nfc_send_command(sam_cmd, 4);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(10)); // Petit délai pour laisser le PN532 répondre
    if (nfc_check_ack() == ESP_OK) {
        ESP_LOGI("NFC", "SAM Config : ACK reçu !");
        vTaskDelay(pdMS_TO_TICKS(100));
        return ESP_OK;
    }

    ESP_LOGE("NFC", "SAM Config : Pas d'ACK !");
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_FAIL;
}

esp_err_t nfc_soft_reset(void) {
    // Trame de réveil / Reset
    uint8_t reset_cmd[] = { 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00 }; 
    ESP_LOGW("NFC", "Reset du module...");

    // 1. Envoi
    esp_err_t ret = i2c_service_write(PN532_ADDR, reset_cmd, sizeof(reset_cmd));
    if (ret != ESP_OK) return ret;

    // 2. Vérification de l'ACK (Le module l'envoie avant de rebooter)
    vTaskDelay(pdMS_TO_TICKS(10));
    if (nfc_check_ack() == ESP_OK) {
        ESP_LOGI("NFC", "Reset : ACK reçu !");
        vTaskDelay(pdMS_TO_TICKS(100)); // Pause nécessaire pour le reboot interne
        return ESP_OK;
    } else {
        ESP_LOGE("NFC", "Reset : Échec (Pas d'ACK)");
        return ESP_FAIL;
    }
}

esp_err_t nfc_read_single_page(uint8_t page, uint8_t *data_out) {
    // Commande PN532 : DataExchange (0x40), Target 1 (0x01), Authentification/Read (0x30), Numéro de page
    uint8_t read_cmd[] = {0x40, 0x01, 0x30, page};
    uint8_t rx[25]; // Buffer suffisant pour header + 4 octets de data + checksum

    // 1. Envoi de la requête de lecture
    if (nfc_send_command(read_cmd, sizeof(read_cmd)) != ESP_OK) return ESP_FAIL;

    vTaskDelay(pdMS_TO_TICKS(5)); 
    if (nfc_check_ack() != ESP_OK) {
        ESP_LOGW("NFC_READ", "ACK non reçu pour la lecture page %d", page);
        return ESP_FAIL;
    }

    // 2. Petit délai pour laisser le PN532 discuter avec le tag par radio
    // 15ms est un bon compromis pour le train Lego
    vTaskDelay(pdMS_TO_TICKS(25));

    // 3. Récupération de la réponse
    if (i2c_service_receive(PN532_ADDR, rx, sizeof(rx)) == ESP_OK) {
        
        // On cherche la signature de réponse DataExchange : D5 41
        for (int i = 0; i < (int)sizeof(rx) - 10; i++) {
            if (rx[i] == 0xD5 && rx[i+1] == 0x41) {
                
                // Le 3ème octet après D5 est le statut (0x00 = succès)
                uint8_t status = rx[i+2];
                if (status != 0x00) {
                    ESP_LOGW("NFC_READ", "Erreur lecture page %d (Status: 0x%02X)", page, status);
                    return ESP_FAIL;
                }

                // Les 4 octets de la page commencent juste après le statut
                memcpy(data_out, &rx[i+3], 4);
                return ESP_OK;
            }
        }
    }

    return ESP_ERR_NOT_FOUND;
}

#define PN532_OFFSET_NBTG       2
#define PN532_OFFSET_UID_LEN    7
#define PN532_OFFSET_UID_START  8
#define NFC_MAX_UID_LEN  10  // La norme ISO max

esp_err_t nfc_parse_passive_target(uint8_t *buffer, size_t len, uint8_t *uid_out, uint8_t *uid_len_out) {
    for (int i = 0; i < (int)len - 1; i++) {
        if (buffer[i] == 0xD5 && buffer[i+1] == 0x4B) {

            // Read tag number
            if (i + PN532_OFFSET_NBTG >= len) { return ESP_ERR_INVALID_SIZE; }
            uint8_t nb_tags = buffer[i + PN532_OFFSET_NBTG];
            
            if (nb_tags > 0) {
                // Read UID lenght
                if (i + PN532_OFFSET_UID_LEN >= len) { return ESP_ERR_INVALID_SIZE; }
                uint8_t actual_uid_len = buffer[i + PN532_OFFSET_UID_LEN];

                // Read UID data
                int uid_start_index = i + PN532_OFFSET_UID_START;
                if (uid_start_index + actual_uid_len > len) { return ESP_ERR_INVALID_SIZE; }
                if (actual_uid_len == 0 || actual_uid_len > NFC_MAX_UID_LEN) { return ESP_ERR_INVALID_ARG; }

                *uid_len_out = actual_uid_len;
                memcpy(uid_out, &buffer[uid_start_index], actual_uid_len);
                
                
                return ESP_OK;
            }
            
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t nfc_scan_tag(nfc_tag_callback_t cb) {
    uint8_t buffer[32];
    uint8_t uid[10];
    uint8_t uid_len = 0;
    uint8_t detect_cmd[] = { 0x4A, 0x01, 0x00 };

    if (nfc_send_command(detect_cmd, 3) != ESP_OK) return ESP_FAIL;

    vTaskDelay(pdMS_TO_TICKS(10)); 
    if (nfc_check_ack() != ESP_OK) {
        return ESP_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(150)); 

    memset(buffer, 0, sizeof(buffer));
    if (i2c_service_receive(PN532_ADDR, buffer, sizeof(buffer)) == ESP_OK) {
        // ESP_LOG_BUFFER_HEX("I2C_RX_RAW", buffer, 32);
        if (nfc_parse_passive_target(buffer, sizeof(buffer), uid, &uid_len) == ESP_OK) {
            
            if (cb != NULL) {
                cb(uid, uid_len);
            }
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

void on_tag_detected(uint8_t *uid, uint8_t len) {
    char uid_str[31] = {0}; 
    for (int i = 0; i < len; i++) {
        sprintf(&uid_str[i * 3], "%02X ", uid[i]);
    }

    // 2. Log d'information
    ESP_LOGI("NFC_CALLBACK", "Tag détecté ! Taille: %d, UID: %s", len, uid_str);

    uint8_t data[4];
    for (int p = 0; p < 10; p++) { // On teste les 10 premières pages
        vTaskDelay(pdMS_TO_TICKS(50));
        if (nfc_read_single_page(p, data) == ESP_OK) {
            ESP_LOGI("NFC_DUMP", "Page %02d : %02X %02X %02X %02X", p, data[0], data[1], data[2], data[3]);
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (nfc_read_single_page(p, data) == ESP_OK) {
                ESP_LOGI("NFC_DUMP", "Page %02d (Retry) : %02X %02X %02X %02X", p, data[0], data[1], data[2], data[3]);
            } else {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Petit repos pour le bus
    }
}

void nfc_polling_task(void *pvParameters) {

    nfc_soft_reset();
    
    if (nfc_sam_config() != ESP_OK) {
        ESP_LOGE("NFC", "Échec de la configuration SAM");
    }

    while (true) {
        nfc_scan_tag(on_tag_detected);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void nfc_service_init(void) {
    xTaskCreate(nfc_polling_task, "nfc_task", 4096, NULL, 5, NULL);
}