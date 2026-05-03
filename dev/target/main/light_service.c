#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "main_config.h"

// ===== GPIO DEFINITIONS =====
// Adjust these pins based on your specific hardware layout

bool lights_enabled = false;

/**
 * @brief Initializes the lighting system GPIOs
 * Configures the front and rear LED pins as outputs with internal pull-ups.
 */
void light_service_init(void) {
    

    // Initialize all lights to OFF (0) state
    light_service_set(false, false, false, false);
    
}

/**
 * @brief Sets the logical level for each train light
 * @param white_front State for front white LEDs
 * @param red_front   State for front red LEDs
 * @param white_rear    State for rear white LEDs
 * @param red_rear     State for rear red LEDs
 */
void light_service_set(bool white_front, bool red_front, bool white_rear, bool red_rear) {
    

    lights_enabled = (white_front || red_front || white_rear || red_rear);
    
    // Log the new state for debugging (Verbosity: Info)
    ESP_LOGI(TAG, "Lights updated - [Front] White:%d, Red:%d | [Rear] White:%d, Red:%d", 
             white_front, red_front, white_rear, red_rear);
}

/** * @brief Returns the global lighting status.
 * @return True if at least one LED is powered ON, false if all LEDs are OFF.
 */
bool light_is_enabled() {
    return lights_enabled;
}