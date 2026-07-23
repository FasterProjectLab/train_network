#include "light_manager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "main_config.h"

#define GPIO_WHITE_FRONT  1
#define GPIO_RED_FRONT    2
#define GPIO_WHITE_REAR   42
#define GPIO_RED_REAR     41

static bool s_lights_enabled = false;

static const char *TAG = "LIGHT_MANAGER";

void light_manager_init(void) {
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
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    light_manager_set(false, false, false, false);
    
    ESP_LOGI(TAG, "Lighting manager initialized on GPIOs: %d, %d, %d, %d", 
             GPIO_WHITE_FRONT, GPIO_RED_FRONT, GPIO_WHITE_REAR, GPIO_RED_REAR);
}

void light_manager_set(bool white_front, bool red_front, bool white_rear, bool red_rear) {
    gpio_set_level(GPIO_WHITE_FRONT, white_front ? 1 : 0);
    gpio_set_level(GPIO_RED_FRONT,   red_front ? 1 : 0);
    gpio_set_level(GPIO_WHITE_REAR,  white_rear ? 1 : 0);
    gpio_set_level(GPIO_RED_REAR,    red_rear ? 1 : 0);

    s_lights_enabled = (white_front || red_front || white_rear || red_rear);
    
    ESP_LOGI(TAG, "Lights updated - [Front] White:%d, Red:%d | [Rear] White:%d, Red:%d", 
             white_front, red_front, white_rear, red_rear);
}

bool light_manager_is_enabled(void) {
    return s_lights_enabled;
}