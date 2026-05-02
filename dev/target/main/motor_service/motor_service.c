#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "main_config.h"
#include <math.h>

#define PCA9685_ADDR          0x40
#define MODE1_REG             0x00
#define PRESCALE_REG          0xFE
#define CHANNEL_BASE_REG      0x06 

#define MOTOR_AIN1_CH         0
#define MOTOR_AIN2_CH         1

esp_err_t pca9685_init() {
    uint8_t setup_data[2];
    
    // 1. Réveil (Sortir du mode SLEEP)
    // On met le bit 4 à 0 dans le registre MODE1
    setup_data[0] = MODE1_REG;
    setup_data[1] = 0x21; // 0x21 = 0010 0001 (Auto-increment enable + Device enabled)
    esp_err_t ret = i2c_service_write(PCA9685_ADDR, setup_data, 2);
    
    // 2. Optionnel : Définir la fréquence (ex: 200Hz pour un moteur DC)
    // Si tu en as besoin, il y a une procédure spécifique (Sleep -> Write Prescale -> Wake)
    
    return ret;
}

esp_err_t pca9685_set_pwm(uint8_t channel, uint16_t duty) {
    if (duty > 4095) duty = 4095;

    uint8_t reg_base = CHANNEL_BASE_REG + (4 * channel);
    uint8_t data[5];
    
    data[0] = reg_base;
    data[1] = 0x00;           // ON Low
    data[2] = 0x00;           // ON High
    data[3] = duty & 0xFF;    // OFF Low
    data[4] = (duty >> 8);    // OFF High

    // Utilise ta fonction i2c_service_write (vu dans tes logs précédents)
    return i2c_service_write(PCA9685_ADDR, data, sizeof(data));
}

void drv8833_set_speed(int16_t speed) {
    if (speed > 0) {
        // Marche Avant : PWM sur AIN1, 0 sur AIN2
        pca9685_set_pwm(MOTOR_AIN1_CH, (uint16_t)speed);
        pca9685_set_pwm(MOTOR_AIN2_CH, 0);
        ESP_LOGD(TAG, "Avant : %d", speed);
    } 
    else if (speed < 0) {
        // Marche Arrière : 0 sur AIN1, PWM sur AIN2
        pca9685_set_pwm(MOTOR_AIN1_CH, 0);
        pca9685_set_pwm(MOTOR_AIN2_CH, (uint16_t)(-speed));
        ESP_LOGD(TAG, "Arrière : %d", -speed);
    } 
    else {
        // Stop (Roue libre)
        pca9685_set_pwm(MOTOR_AIN1_CH, 0);
        pca9685_set_pwm(MOTOR_AIN2_CH, 0);
        ESP_LOGI(TAG, "Stop");
    }
}

void drv8833_set_speed_percent(int8_t percent) {
    // 1. Limiter l'entrée entre -100 et 100 par sécurité
    if (percent > 100) percent = 100;
    if (percent < -100) percent = -100;

    // 2. Calculer la valeur PWM (Règle de trois : 4095 / 100 = 40.95)
    // On utilise un float temporaire pour la précision avant de repasser en int
    int16_t pca_speed = (int16_t)(percent * 40.95f);

    // 3. Appeler la fonction de pilotage existante
    drv8833_set_speed(pca_speed);
    
    ESP_LOGI(TAG, "Vitesse : %d%% (Valeur PCA : %d)", percent, pca_speed);
}