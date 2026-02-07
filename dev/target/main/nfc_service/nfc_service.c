#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "main_config.h"
#include "nfc_service.h"

static QueueHandle_t nfc_queue = NULL;

#define PN532_4B_NB_TAGS_OFFSET   (PN532_DATA_START_OFFSET + 1) // Index 8
#define PN532_4B_UID_LEN_OFFSET   (PN532_DATA_START_OFFSET + 6) // Index 13
#define PN532_4B_UID_START_OFFSET (PN532_DATA_START_OFFSET + 7) // Index 14

esp_err_t nfc_read_page(uint8_t page);
void nfc_parse_ndef_payload(uint8_t *buf);

void parse_4B_response(uint8_t *buf) {
    uint8_t nb_tags = buf[PN532_4B_NB_TAGS_OFFSET];
    
    if (nb_tags > 0) {
        uint8_t uid_len = buf[PN532_4B_UID_LEN_OFFSET];
        uint8_t *uid_ptr = &buf[PN532_4B_UID_START_OFFSET];

        ESP_LOGI("NFC", "Tag trouvé ! Taille UID: %d", uid_len);
        ESP_LOG_BUFFER_HEX("UID", uid_ptr, uid_len);

        
    } else {
        ESP_LOGW("NFC", "La trame 4B indique 0 tags (erreur ou retrait rapide)");
    }
}

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

esp_err_t nfc_read_page(uint8_t page) {
    // 0x40: InDataExchange
    // 0x01: Target 1
    // 0x30: Mifare Read
    // page: Numéro de la page
    uint8_t read_cmd[] = { 0x40, 0x01, 0x30, page };
    
    ESP_LOGI("NFC", "Lecture de la page %d...", page);
    return nfc_send_command(read_cmd, sizeof(read_cmd));
}

esp_err_t nfc_sam_config(void) {
    uint8_t sam_cmd[] = { 0x14, 0x01, 0x14, 0x01 };
    ESP_LOGI("NFC", "Configuration du SAM...");
    
    esp_err_t ret = nfc_send_command(sam_cmd, 4);
    if (ret != ESP_OK) return ret;
    return ESP_OK;
}

esp_err_t nfc_soft_reset(void) {
    // Trame de réveil / Reset
    uint8_t reset_cmd[] = { 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00 }; 
    ESP_LOGW("NFC", "Reset du module...");

    // 1. Envoi
    esp_err_t ret = i2c_service_write(PN532_ADDR, reset_cmd, sizeof(reset_cmd));
    if (ret != ESP_OK) return ret;
    return ESP_OK;

}

esp_err_t nfc_scan_tag() {
    uint8_t detect_cmd[] = { 0x4A, 0x01, 0x00 };

    if (nfc_send_command(detect_cmd, sizeof(detect_cmd)) != ESP_OK) return ESP_FAIL;
    return ESP_OK;
}


// Callback d'interruption (ISR)
static void IRAM_ATTR nfc_gpio_isr_handler(void* arg) {
    uint32_t pin = (uint32_t) arg;
    xQueueSendFromISR(nfc_queue, &pin, NULL);
}

void nfc_process_irq_buffer(uint8_t *buf) {
    if (!IS_PN532_FRAME(buf)) {
        ESP_LOGE("NFC", "Trame invalide (Bruit ou erreur de bus)");
        return;
    }

    if (IS_PN532_ACK(buf)) {
        ESP_LOGI("NFC", ">>> ACK Reçu");
        return;
    }

    if (IS_PN532_DATA_RESP(buf)) {
        uint8_t data_len = buf[PN532_LEN_OFFSET];
        uint8_t command_code = buf[PN532_DATA_START_OFFSET]; // Ex: 0x4B pour InListPassiveTarget

        ESP_LOGI("NFC", ">>> DATA Reçue (Len: %d, Cmd: 0x%02X)", data_len, command_code);
        switch (command_code) {
            case 0x15: // Réponse à SAMConfiguration
                ESP_LOGI("NFC", "SAM Configuré. Lancement du Scan...");
                nfc_scan_tag(); // On enchaîne ici
                break;
            case 0x4B:
                parse_4B_response(buf);
                nfc_read_page(4);
                break;
            case 0x41: // Résultat de la lecture (InDataExchange)
                nfc_parse_ndef_payload(buf);
                // Maintenant on peut réarmer le scan pour la suite
                vTaskDelay(pdMS_TO_TICKS(500));
                nfc_scan_tag();
                break;
        }
        
    }
}

// Tâche de fond qui traite l'interruption
static void nfc_worker_task(void* arg) {
    uint32_t io_num;
    uint8_t rx_buf[64];
    ESP_LOGI(TAG, "Tâche NFC démarrée.");
    
    for(;;) {
        // Attend un signal de la queue (bloquant, 0% CPU en attente)
        if(xQueueReceive(nfc_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "IRQ détectée sur GPIO %ld : Le PN532 a des données !", io_num);
            
            memset(rx_buf, 0, sizeof(rx_buf));
            if (i2c_service_receive(PN532_ADDR, rx_buf, sizeof(rx_buf)) != ESP_OK) continue;

            ESP_LOG_BUFFER_HEX("I2C_RX_RAW", rx_buf, 64);
            nfc_process_irq_buffer(rx_buf);
        }
    }
}

esp_err_t nfc_service_init_IRQ() {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,    // IRQ PN532 passe de High à Low
        .pin_bit_mask = (1ULL << NFC_IRQ_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,                   // Crucial si pas de pull-up externe
    };
    gpio_config(&io_conf);

    nfc_queue = xQueueCreate(10, sizeof(uint32_t));
    if (nfc_queue == NULL) return ESP_FAIL;

    xTaskCreate(nfc_worker_task, "nfc_worker", 3072, NULL, 10, NULL);

    gpio_install_isr_service(0); 
    gpio_isr_handler_add(NFC_IRQ_PIN, nfc_gpio_isr_handler, (void*) NFC_IRQ_PIN);
    return ESP_OK;
}

esp_err_t nfc_service_init(void) {
    nfc_service_init_IRQ();

    if (nfc_sam_config() != ESP_OK) {
        ESP_LOGE("NFC", "Échec de la configuration SAM");
    }
    ESP_LOGI(TAG, "Service NFC initialisé avec succès.");
    return ESP_OK;
}

void nfc_parse_ndef_payload(uint8_t *buf) {
    // Vérification du statut de lecture (index 8)
    if (buf[PN532_41_STATUS_OFFSET] != 0x00) {
        ESP_LOGE("NFC", "Erreur InDataExchange (Status: 0x%02X)", buf[PN532_41_STATUS_OFFSET]);
        return;
    }

    uint8_t *data = &buf[PN532_41_PAYLOAD_OFFSET];

    // 1. Chercher le TLV NDEF (Type 0x03)
    if (data[0] == NDEF_TLV_TYPE_NDEF) {
        uint8_t ndef_len = data[1];
        uint8_t *record = &data[2];

        // 2. Vérifier si c'est un Record de type TEXT ('T')
        // d1 01 [len] 54
        if (record[3] == NDEF_RECORD_TYPE_TEXT) {
            uint8_t payload_len = record[2];
            uint8_t *payload = &record[4];
            
            // Dans un Text Record, payload[0] contient la longueur du code langue (ex: 0x02 pour "en")
            uint8_t lang_len = payload[0] & 0x3F;
            uint8_t text_offset = 1 + lang_len;
            uint8_t text_len = payload_len - text_offset;

            char msg[32] = {0};
            memcpy(msg, &payload[text_offset], (text_len > 31) ? 31 : text_len);
            
            ESP_LOGW("NFC_NDEF", "Message lu : [%s]", msg);
        }
    } else {
        // Si ce n'est pas du NDEF, on affiche juste les octets bruts 
        ESP_LOGI("NFC_RAW", "Données brutes de la page :");
        ESP_LOG_BUFFER_HEX("PAGE_DATA", data, 16);
    }
}