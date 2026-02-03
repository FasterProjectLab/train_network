#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "main_config.h"

// ===== GPIO DEFINITIONS =====
// Adjust these pins based on your specific hardware layout
#define GPIO_WHITE_FRONT  1
#define GPIO_RED_FRONT    2
#define GPIO_WHITE_REAR   42
#define GPIO_RED_REAR     41

/**
 * @brief Initializes the lighting system GPIOs
 * Configures the front and rear LED pins as outputs with internal pull-ups.
 */
void light_service_init(void) {
    // Group configuration for all 4 lighting GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_WHITE_FRONT) | 
                        (1ULL << GPIO_RED_FRONT) | 
                        (1ULL << GPIO_WHITE_REAR) | 
                        (1ULL << GPIO_RED_REAR),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    // Apply configuration and check for errors
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Initialize all lights to OFF (0) state
    light_service_set(false, false, false, false);
    
    ESP_LOGI(TAG, "Lighting service initialized on GPIOs: %d, %d, %d, %d", 
             GPIO_WHITE_FRONT, GPIO_RED_FRONT, GPIO_WHITE_REAR, GPIO_RED_REAR);
}

/**
 * @brief Sets the logical level for each train light
 * @param white_front State for front white LEDs
 * @param red_front   State for front red LEDs
 * @param white_rear    State for rear white LEDs
 * @param red_rear     State for rear red LEDs
 */
void light_service_set(bool white_front, bool red_front, bool white_rear, bool red_rear) {
    // Set individual GPIO levels based on boolean input
    gpio_set_level(GPIO_WHITE_FRONT, white_front ? 1 : 0);
    gpio_set_level(GPIO_RED_FRONT,   red_front ? 1 : 0);
    gpio_set_level(GPIO_WHITE_REAR,  white_rear ? 1 : 0);
    gpio_set_level(GPIO_RED_REAR,    red_rear ? 1 : 0);
    
    // Log the new state for debugging (Verbosity: Info)
    ESP_LOGI(TAG, "Lights updated - [Front] White:%d, Red:%d | [Rear] White:%d, Red:%d", 
             white_front, red_front, white_rear, red_rear);
}