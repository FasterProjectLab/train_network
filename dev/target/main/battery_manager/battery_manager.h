#pragma once

#include "esp_err.h"

/**
 * @brief Initialise le périphérique ADC pour la mesure de la batterie (GPIO1)
 * @return ESP_OK si tout s'est bien passé
 */
esp_err_t battery_manager_init(void);

/**
 * @brief Lit la tension actuelle de la batterie
 * @return Tension de la batterie en Volts (0.0f en cas d'erreur)
 */
float battery_manager_get_voltage(void);