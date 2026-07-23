#ifndef NFC_MANAGER_H
#define NFC_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Configuration Hardware --- */
#define PN532_I2C_ADDR          0x24
#define NFC_IRQ_PIN             GPIO_NUM_42

/* --- Offsets de trame PN532 --- */
#define PN532_I2C_STAT_OFFSET   0  // 0x01 = Ready, 0x00 = Not Ready
#define PN532_PREAMBLE_OFFSET   1  // 0x00
#define PN532_STARTCODE_OFFSET  3  // 0xFF
#define PN532_LEN_OFFSET        4  // Longueur des données (TFI + PD)
#define PN532_LCS_OFFSET        5  // Checksum de la longueur
#define PN532_TFI_OFFSET        6  // Frame Identifier (D4=Host->PN, D5=PN->Host)
#define PN532_DATA_START_OFFSET 7  // Début des données utiles (Packet Data)

/* --- Valeurs constantes PN532 --- */
#define PN532_PREAMBLE          0x00
#define PN532_STARTCODE         0xFF
#define PN532_TFI_HOST_TO_PN    0xD4
#define PN532_TFI_PN_TO_HOST    0xD5

#define PN532_ACK_LEN           6   // Taille d'un ACK (sans le statut I2C)
#define PN532_FRAME_OVERHEAD    8   // Octets hors DATA (Preamble(3)+LEN+LCS+TFI ... DCS+Postamble)

/* --- Offsets spécifiques aux commandes --- */
// Offset pour InDataExchange (Réponse 0x41)
#define PN532_41_STATUS_OFFSET  (PN532_DATA_START_OFFSET + 1) // Index 8 (0x00 = OK)
#define PN532_41_PAYLOAD_OFFSET (PN532_DATA_START_OFFSET + 2) // Index 9 (Début des données du tag)

/* --- Constantes NDEF --- */
#define NDEF_TLV_TYPE_NDEF       0x03
#define NDEF_TLV_TYPE_TERMINATOR 0xFE

#define NDEF_RECORD_HEADER_LEN  4    // D1 01 [LEN] 54
#define NDEF_RECORD_TYPE_TEXT   0x54 // 'T' en ASCII

/* --- Macros de vérification --- */
#define IS_PN532_FRAME(buf) \
    ((buf)[PN532_PREAMBLE_OFFSET] == 0x00 && \
     (buf)[PN532_PREAMBLE_OFFSET + 1] == 0x00 && \
     (buf)[PN532_STARTCODE_OFFSET] == 0xFF)

#define IS_PN532_ACK(buf) \
    (IS_PN532_FRAME(buf) && \
     (buf)[PN532_LEN_OFFSET] == 0x00 && \
     (buf)[PN532_LCS_OFFSET] == 0xFF)

#define IS_PN532_DATA_RESP(buf) \
    (IS_PN532_FRAME(buf) && \
     (buf)[PN532_TFI_OFFSET] == PN532_TFI_PN_TO_HOST)

#ifdef __cplusplus
}
#endif

#endif // NFC_MANAGER_H