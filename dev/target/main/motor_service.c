#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "main_config.h"

// ===== GPIO CONFIGURATION =====
#define MOTOR_AIN1_GPIO 47
#define MOTOR_AIN2_GPIO 21

// ===== PWM CONFIGURATION =====
#define MOTOR_PWM_FREQ       20000  // 20 kHz frequency
#define MOTOR_PWM_RES        LEDC_TIMER_10_BIT
#define MOTOR_PWM_MAX_DUTY   ((1 << 10) - 1) // 1023 for 10-bit resolution

#define MOTOR_PWM_MODE       LEDC_LOW_SPEED_MODE
#define MOTOR_PWM_TIMER      LEDC_TIMER_1
#define MOTOR_PWM_CH_AIN1    LEDC_CHANNEL_2
#define MOTOR_PWM_CH_AIN2    LEDC_CHANNEL_3

// State management variables
static bool current_dir = true;         // true = forward, false = backward
static int64_t last_stop_time = 0;      // In microseconds (uptime)
static bool is_currently_moving = false;

/**
 * @brief Returns the current set direction of the motor
 * @return true for forward, false for backward
 */
bool motor_get_current_direction(void) {
    return current_dir;
}

/**
 * @brief Initializes the LEDC peripheral for DC motor PWM control
 */
void motor_service_init(void)
{
    // Configure PWM Timer
    ledc_timer_config_t timer_cfg = {
        .speed_mode       = MOTOR_PWM_MODE,
        .timer_num        = MOTOR_PWM_TIMER,
        .duty_resolution  = MOTOR_PWM_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    // Configure Channel AIN1 (Forward/Phase A)
    ledc_channel_config_t ch1 = {
        .gpio_num   = MOTOR_AIN1_GPIO,
        .speed_mode = MOTOR_PWM_MODE,
        .channel    = MOTOR_PWM_CH_AIN1,
        .timer_sel  = MOTOR_PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch1));

    // Configure Channel AIN2 (Backward/Phase B)
    ledc_channel_config_t ch2 = {
        .gpio_num   = MOTOR_AIN2_GPIO,
        .speed_mode = MOTOR_PWM_MODE,
        .channel    = MOTOR_PWM_CH_AIN2,
        .timer_sel  = MOTOR_PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch2));

    ESP_LOGI("MOTOR", "Motor service initialized (20kHz, 10-bit resolution)");
}

/**
 * @brief Sets motor speed and direction with safety interlocking
 * @param duty Percentage of power (0-100)
 * @param forward Direction flag
 */
void motor_service_set_speed(uint8_t duty, bool forward)
{
    int64_t now = esp_timer_get_time();

    // Input sanitization
    if (duty > 100) duty = 100;

    // 1. Safety Interlock: Prevent rapid direction reversal (Back-EMF protection)
    if (forward != current_dir && duty > 0) {
        // Calculate elapsed time since last full stop in milliseconds
        uint32_t elapsed_ms = (uint32_t)((now - last_stop_time) / 1000);

        // If motor is still moving or stopped for less than 3 seconds
        if (is_currently_moving || elapsed_ms < 3000) {
            ESP_LOGW("MOTOR", "Reversal rejected! Must wait 3s at full stop (Elapsed: %lu ms)", elapsed_ms);
            // Force zero duty to prevent hardware damage
            duty = 0; 
        } else {
            // Safe to switch direction
            current_dir = forward;
            ESP_LOGI("MOTOR", "Direction changed successfully");
        }
    }

    // 2. Movement state tracking and stop timer management
    if (duty == 0) {
        if (is_currently_moving) {
            last_stop_time = now; // Start the 3-second safety countdown
            is_currently_moving = false;
            ESP_LOGI("MOTOR", "Motor stopped, 3s safety timer started");
        }
    } else {
        is_currently_moving = true;
        // Apply direction if not blocked by safety logic above
        current_dir = forward; 
    }

    // Convert percentage (0-100) to LEDC duty value (0-1023)
    uint32_t pwm_duty = (duty * MOTOR_PWM_MAX_DUTY) / 100;

    if (forward) {
        // AIN1 active PWM, AIN2 grounded
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1, pwm_duty);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1);

        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2, 0);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2);
    } else {
        // AIN2 active PWM, AIN1 grounded
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1, 0);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1);

        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2, pwm_duty);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2);
    }
}

/**
 * @brief Immediate emergency stop (sets both PWM channels to 0)
 */
void motor_service_stop(void)
{
    ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1, 0);
    ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1);

    ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2, 0);
    ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2);
    
    is_currently_moving = false;
    last_stop_time = esp_timer_get_time();
    ESP_LOGI("MOTOR", "Emergency stop triggered");
}