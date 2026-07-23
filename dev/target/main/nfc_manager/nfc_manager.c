#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "main_config.h"
#include "nfc_manager.h"

static const char *TAG = "NFC_MGR";
static QueueHandle_t nfc_queue = NULL;

#define PN532_4B_NB_TAGS_OFFSET   (PN532_DATA_START_OFFSET + 1) // Index 8
#define PN532_4B_UID_LEN_OFFSET   (PN532_DATA_START_OFFSET + 6) // Index 13
#define PN532_4B_UID_START_OFFSET (PN532_DATA_START_OFFSET + 7) // Index 14

// Prototypes internes
esp_err_t nfc_read_page(uint8_t page);
void nfc_parse_ndef_payload(uint8_t *buf);

void parse_4B_response(uint8_t *buf) {
    uint8_t nb_tags = buf[PN532_4B_NB_TAGS_OFFSET];
    
    if (nb_tags > 0) {
        uint8_t uid_len = buf[PN532_4B_UID_LEN_OFFSET];
        uint8_t *uid_ptr = &buf[PN532_4B_UID_START_OFFSET];

        ESP_LOGI(TAG, "Tag trouvé ! Taille UID: %d", uid_len);
        ESP_LOG_BUFFER_HEX("UID", uid_ptr, uid_len);
    } else {
        ESP_LOGW(TAG, "La trame 4B indique 0 tags (erreur ou retrait rapide)");
    }
}

esp_err_t nfc_send_command(uint8_t *cmd, size_t cmd_len) {
    // Taille max de trame raisonnable pour éviter les VLA sur la pile
    uint8_t packet[64]; 
    if (cmd_len + 8 > sizeof(packet)) {
        ESP_LOGE(TAG, "Commande trop longue pour le buffer de sortie");
        return ESP_ERR_NO_MEM;
    }

    uint8_t len = cmd_len + 1;
    uint8_t lcs = (uint8_t)(~len + 1);
    
    packet[0] = 0x00; 
    packet[1] = 0x00; 
    packet[2] = 0xFF;
    packet[3] = len; 
    packet[4] = lcs; 
    packet[5] = PN532_TFI_HOST_TO_PN;

    uint8_t dcs_sum = PN532_TFI_HOST_TO_PN;
    for (size_t i = 0; i < cmd_len; i++) {
        packet[6 + i] = cmd[i];
        dcs_sum += cmd[i];
    }
    packet[6 + cmd_len] = (uint8_t)(~dcs_sum + 1);
    packet[7 + cmd_len] = 0x00;

    return i2c_manager_write(PN532_I2C_ADDR, packet, cmd_len + 8);
}

esp_err_t nfc_read_page(uint8_t page) {
    uint8_t read_cmd[] = { 0x40, 0x01, 0x30, page };
    ESP_LOGI(TAG, "Lecture de la page %d...", page);
    return nfc_send_command(read_cmd, sizeof(read_cmd));
}

esp_err_t nfc_sam_config(void) {
    uint8_t sam_cmd[] = { 0x14, 0x01, 0x14, 0x01 };
    ESP_LOGI(TAG, "Configuration du SAM...");
    return nfc_send_command(sam_cmd, sizeof(sam_cmd));
}

esp_err_t nfc_soft_reset(void) {
    uint8_t reset_cmd[] = { 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00 }; 
    ESP_LOGW(TAG, "Reset du module...");
    return i2c_manager_write(PN532_I2C_ADDR, reset_cmd, sizeof(reset_cmd));
}

esp_err_t nfc_scan_tag(void) {
    uint8_t detect_cmd[] = { 0x4A, 0x01, 0x00 };
    return nfc_send_command(detect_cmd, sizeof(detect_cmd));
}

// ISR Callback (Exécutée en Contexte d'Interruption)
static void IRAM_ATTR nfc_gpio_isr_handler(void* arg) {
    uint32_t pin = (uint32_t) arg;
    xQueueSendFromISR(nfc_queue, &pin, NULL);
}

void nfc_process_irq_buffer(uint8_t *buf) {
    if (!IS_PN532_FRAME(buf)) {
        ESP_LOGE(TAG, "Trame invalide (Bruit ou erreur de bus)");
        return;
    }

    if (IS_PN532_ACK(buf)) {
        ESP_LOGI(TAG, ">>> ACK Reçu");
        return;
    }

    if (IS_PN532_DATA_RESP(buf)) {
        uint8_t data_len = buf[PN532_LEN_OFFSET];
        uint8_t command_code = buf[PN532_DATA_START_OFFSET];

        ESP_LOGI(TAG, ">>> DATA Reçue (Len: %d, Cmd: 0x%02X)", data_len, command_code);
        switch (command_code) {
            case 0x15: // SAMConfiguration Response
                ESP_LOGI(TAG, "SAM Configuré. Lancement du Scan...");
                nfc_scan_tag();
                break;

            case 0x4B: // InListPassiveTarget Response
                parse_4B_response(buf);
                nfc_read_page(4);
                break;

            case 0x41: // InDataExchange Response
                nfc_parse_ndef_payload(buf);
                vTaskDelay(pdMS_TO_TICKS(500));
                nfc_scan_tag();
                break;

            default:
                ESP_LOGW(TAG, "Commande non gérée : 0x%02X", command_code);
                break;
        }
    }
}

static void nfc_worker_task(void* arg) {
    uint32_t io_num;
    uint8_t rx_buf[64];
    ESP_LOGI(TAG, "Tâche NFC démarrée.");
    
    for(;;) {
        if (xQueueReceive(nfc_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "IRQ détectée sur GPIO %ld", io_num);
            
            memset(rx_buf, 0, sizeof(rx_buf));
            if (i2c_manager_receive(PN532_I2C_ADDR, rx_buf, sizeof(rx_buf)) == ESP_OK) {
                ESP_LOG_BUFFER_HEX("I2C_RX_RAW", rx_buf, sizeof(rx_buf));
                nfc_process_irq_buffer(rx_buf);
            }
        }
    }
}

esp_err_t nfc_manager_init_IRQ(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,    
        .pin_bit_mask = (1ULL << NFC_IRQ_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,                   
    };
    gpio_config(&io_conf);

    nfc_queue = xQueueCreate(10, sizeof(uint32_t));
    if (nfc_queue == NULL) return ESP_FAIL;

    // Légère augmentation de la pile du worker de 3072 à 4096 pour être tranquille avec ESP_LOG
    xTaskCreate(nfc_worker_task, "nfc_worker", 4096, NULL, 10, NULL);

    gpio_install_isr_service(0); 
    gpio_isr_handler_add(NFC_IRQ_PIN, nfc_gpio_isr_handler, (void*) NFC_IRQ_PIN);
    return ESP_OK;
}

esp_err_t nfc_manager_init(void) {
    if (nfc_manager_init_IRQ() != ESP_OK) {
        ESP_LOGE(TAG, "Échec init IRQ");
        return ESP_FAIL;
    }

    if (nfc_sam_config() != ESP_OK) {
        ESP_LOGE(TAG, "Échec de la configuration SAM");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Service NFC initialisé avec succès.");
    return ESP_OK;
}

void nfc_parse_ndef_payload(uint8_t *buf) {
    if (buf[PN532_41_STATUS_OFFSET] != 0x00) {
        ESP_LOGE(TAG, "Erreur InDataExchange (Status: 0x%02X)", buf[PN532_41_STATUS_OFFSET]);
        return;
    }

    uint8_t *data = &buf[PN532_41_PAYLOAD_OFFSET];

    // Vérification TLV NDEF (Type 0x03)
    if (data[0] == NDEF_TLV_TYPE_NDEF) {
        uint8_t *record = &data[2];

        // Vérification sécurisée du record NDEF Text ('T')
        if (record[3] == NDEF_RECORD_TYPE_TEXT) {
            uint8_t payload_len = record[2];
            uint8_t *payload = &record[4];
            
            uint8_t lang_len = payload[0] & 0x3F;
            uint8_t text_offset = 1 + lang_len;

            if (payload_len > text_offset) {
                uint8_t text_len = payload_len - text_offset;
                char msg[32] = {0};
                
                size_t copy_len = (text_len < sizeof(msg) - 1) ? text_len : sizeof(msg) - 1;
                memcpy(msg, &payload[text_offset], copy_len);
                
                ESP_LOGW("NFC_NDEF", "Message lu : [%s]", msg);
            }
        }
    } else {
        ESP_LOGI("NFC_RAW", "Données brutes de la page :");
        ESP_LOG_BUFFER_HEX("PAGE_DATA", data, 16);
    }
}