#include "turnout_manager.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "turnout_mgr";

typedef struct {
    int id;
    int gpio_pin;
    ledc_channel_t ledc_channel;
    uint32_t duty_normal;    
    uint32_t duty_divergent; 
    turnout_position_t current_position;
} turnout_t;

// Vos aiguillages configurés
static turnout_t turnouts[] = {
    { .id = 0, .gpio_pin = 18, .ledc_channel = LEDC_CHANNEL_0, .duty_normal = 800, .duty_divergent = 630, .current_position = TURNOUT_POSITION_NORMAL },
    { .id = 1, .gpio_pin = 19, .ledc_channel = LEDC_CHANNEL_1, .duty_normal = 800, .duty_divergent = 630, .current_position = TURNOUT_POSITION_NORMAL }
};

#define TURNOUTS_COUNT (sizeof(turnouts) / sizeof(turnouts[0]))

void turnout_manager_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_14_BIT, 
        .freq_hz          = 50,                
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    for (int i = 0; i < TURNOUTS_COUNT; i++) {
        ledc_channel_config_t ledc_channel = {
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = turnouts[i].ledc_channel,
            .timer_sel      = LEDC_TIMER_0,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = turnouts[i].gpio_pin,
            .duty           = turnouts[i].duty_normal, 
            .hpoint         = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }
    ESP_LOGI(TAG, "%d aiguillages initialisés.", TURNOUTS_COUNT);
}

// --- INTERFACE CLAIRE D'ACTIONNEMENT DIRECT ---
bool turnout_manager_set_position(int turnout_id, turnout_position_t position)
{
    ESP_LOGI(TAG, "Demande de changement d'état : Aiguillage %d -> %s", 
             turnout_id, (position == TURNOUT_POSITION_NORMAL) ? "NORMAL" : "DIVERGENT");

    turnout_t *target_turnout = NULL;
    for (int i = 0; i < TURNOUTS_COUNT; i++) {
        if (turnouts[i].id == turnout_id) {
            target_turnout = &turnouts[i];
            break;
        }
    }

    if (!target_turnout) {
        ESP_LOGE(TAG, "set_position manqué : ID %d introuvable.", turnout_id);
        return false;
    }

    // [OPTIONNEL] Log pour éviter un mouvement inutile si l'état est déjà identique
    if (target_turnout->current_position == position) {
        ESP_LOGW(TAG, "Aiguillage %d est déjà dans l'état demandé. Action ignorée.", turnout_id);
        return true; 
    }

    uint32_t target_duty = 0;
    if (position == TURNOUT_POSITION_NORMAL) {
        target_duty = target_turnout->duty_normal;
        ESP_LOGI(TAG, "Aiguillage %d -> NORMALE (Physique, Duty: %lu)", turnout_id, (unsigned long)target_duty);
    } else if (position == TURNOUT_POSITION_DIVERGENT) {
        target_duty = target_turnout->duty_divergent;
        ESP_LOGI(TAG, "Aiguillage %d -> DIVERGENTE (Physique, Duty: %lu)", turnout_id, (unsigned long)target_duty);
    } else {
        ESP_LOGE(TAG, "set_position manqué : État %d inconnu ou invalide.", position);
        return false;
    }
    
    target_turnout->current_position = position;

    // Configuration et mise à jour du signal PWM (Servomoteur/Relais)
    ESP_LOGD(TAG, "Mise à jour PWM sur canal %d...", target_turnout->ledc_channel);
    
    esp_err_t err_set = ledc_set_duty(LEDC_LOW_SPEED_MODE, target_turnout->ledc_channel, target_duty);
    esp_err_t err_upd = ledc_update_duty(LEDC_LOW_SPEED_MODE, target_turnout->ledc_channel);

    if (err_set != ESP_OK || err_upd != ESP_OK) {
        ESP_LOGE(TAG, "Erreur matérielle LEDC sur l'aiguillage %d (Set: %d, Upd: %d)", turnout_id, err_set, err_upd);
        return false;
    }

    ESP_LOGI(TAG, "Aiguillage %d appliqué avec succès de manière matérielle.", turnout_id);
    return true;
}

turnout_position_t turnout_manager_get_position(int turnout_id) {
    // On parcourt ton vrai tableau pour trouver l'aiguillage demandé
    for (int i = 0; i < TURNOUTS_COUNT; i++) {
        if (turnouts[i].id == turnout_id) {
            return turnouts[i].current_position;
        }
    }
    
    ESP_LOGE(TAG, "ID d'aiguillage introuvable pour la lecture : %d", turnout_id);
    return TURNOUT_POSITION_UNKNOWN;
}

int turnout_manager_get_count(void) {
    return TURNOUTS_COUNT;
}