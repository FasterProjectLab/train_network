#include "battery_manager.h"
#include <math.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "BATTERY_MANAGER";

#define DIVIDER_RATIO 3.38f 

static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
static adc_cali_handle_t s_adc_cali_handle = NULL;

esp_err_t battery_manager_init(void) {
    if (s_adc1_handle != NULL) return ESP_OK;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, ADC_CHANNEL_0, &config));

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &s_adc_cali_handle) == ESP_OK) {
        ESP_LOGI(TAG, "ADC Calibration ready");
    }
#endif

    return ESP_OK;
}

float battery_manager_get_voltage(void) {
    if (s_adc1_handle == NULL) return 0.0f;

    int raw_val = 0;
    int voltage_mv = 0;

    if (adc_oneshot_read(s_adc1_handle, ADC_CHANNEL_0, &raw_val) == ESP_OK) {
        float measured_v = 0.0f;

        if (s_adc_cali_handle != NULL) {
            adc_cali_raw_to_voltage(s_adc_cali_handle, raw_val, &voltage_mv);
            measured_v = (voltage_mv / 1000.0f) * DIVIDER_RATIO;
        } else {
            measured_v = (raw_val / 4095.0f) * 3.3f * DIVIDER_RATIO;
        }

        // Arrondi propre à 2 décimales (ex: 8.35 V)
        return roundf(measured_v * 100.0f) / 100.0f;
    }

    return 0.0f;
}