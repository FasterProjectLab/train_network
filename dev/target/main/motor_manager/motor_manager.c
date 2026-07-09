#include "motor_manager.h"
#include "light_manager/light_manager.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "main_config.h"

#define MOTOR_AIN1_GPIO 47
#define MOTOR_AIN2_GPIO 21

#define MOTOR_PWM_FREQ       20000  
#define MOTOR_PWM_RES        LEDC_TIMER_10_BIT
#define MOTOR_PWM_MAX_DUTY   ((1 << 10) - 1) 

#define MOTOR_PWM_MODE       LEDC_LOW_SPEED_MODE
#define MOTOR_PWM_TIMER      LEDC_TIMER_1
#define MOTOR_PWM_CH_AIN1    LEDC_CHANNEL_2
#define MOTOR_PWM_CH_AIN2    LEDC_CHANNEL_3

static bool s_current_dir = true;       
static uint8_t s_current_speed = 0;       
static int64_t s_last_stop_time = 0;      
static bool s_is_currently_moving = false;

bool motor_manager_get_current_direction(void) {
    return s_current_dir;
}

uint8_t motor_manager_get_current_speed(void) {
    return s_current_speed;
}


void motor_manager_set_direction(bool forward) {
    if (s_current_dir == forward) return;

    s_current_dir = forward;
    
    ESP_LOGI("MOTOR", "Motor direction changed to: %s", s_current_dir ? "FORWARD" : "BACKWARD");

    if (light_manager_is_enabled()) {
        light_manager_update_by_current_dir();
    }
}

void motor_manager_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode       = MOTOR_PWM_MODE,
        .timer_num        = MOTOR_PWM_TIMER,
        .duty_resolution  = MOTOR_PWM_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch1 = {
        .gpio_num   = MOTOR_AIN1_GPIO,
        .speed_mode = MOTOR_PWM_MODE,
        .channel    = MOTOR_PWM_CH_AIN1,
        .timer_sel  = MOTOR_PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch1));

    ledc_channel_config_t ch2 = {
        .gpio_num   = MOTOR_AIN2_GPIO,
        .speed_mode = MOTOR_PWM_MODE,
        .channel    = MOTOR_PWM_CH_AIN2,
        .timer_sel  = MOTOR_PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch2));

    ESP_LOGI("MOTOR", "Motor manager initialized (20kHz, 10-bit resolution)");
}

void motor_manager_set_speed(uint8_t duty, bool forward)
{
    int64_t now = esp_timer_get_time();

    if (duty > 100) duty = 100;

    if (forward != s_current_dir && duty > 0) {
        uint32_t elapsed_ms = (uint32_t)((now - s_last_stop_time) / 1000);

        if (s_is_currently_moving || elapsed_ms < 3000) {
            ESP_LOGW("MOTOR", "Reversal rejected! Must wait 3s at full stop (Elapsed: %lu ms)", elapsed_ms);
            duty = 0; 
        } else {
            motor_manager_set_direction(forward);
            ESP_LOGI("MOTOR", "Direction changed successfully");
        }
    }

    if (duty == 0) {
        if (s_is_currently_moving) {
            s_last_stop_time = now; 
            s_is_currently_moving = false;
            ESP_LOGI("MOTOR", "Motor stopped, 3s safety timer started");
        }
    } else {
        s_is_currently_moving = true;
        motor_manager_set_direction(forward);
    }

    s_current_speed = duty;

    uint32_t pwm_duty = (duty * MOTOR_PWM_MAX_DUTY) / 100;

    if (forward) {
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1, pwm_duty);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1);

        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2, 0);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2);
    } else {
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1, 0);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1);

        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2, pwm_duty);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2);
    }
}

void motor_manager_stop(void)
{
    ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1, 0);
    ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN1);

    ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2, 0);
    ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_AIN2);
    
    s_is_currently_moving = false;
    s_current_speed = 0;
    s_last_stop_time = esp_timer_get_time();
    ESP_LOGI("MOTOR", "Emergency stop triggered");
}