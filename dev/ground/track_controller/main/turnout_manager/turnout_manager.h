#ifndef TURNOUT_MANAGER_H
#define TURNOUT_MANAGER_H

#include "cJSON.h"

/**
 * @brief Les deux états physiques possibles pour un aiguillage.
 */
typedef enum {
    TURNOUT_POSITION_NORMAL = 0,
    TURNOUT_POSITION_DIVERGENT,
    TURNOUT_POSITION_UNKNOWN
} turnout_position_t;

/**
 * @brief Initialise les périphériques PWM pour tous les servos d'aiguillage.
 */
void turnout_manager_init(void);

/**
 * @brief Change l'état d'un aiguillage spécifique de manière directe.
 * * @param turnout_id L'identifiant de l'aiguillage (0, 1, etc.)
 * @param state      L'état ciblé (TURNOUT_POSITION_NORMAL ou TURNOUT_POSITION_DIVERGENT)
 * @return true si l'action a été exécutée, false si l'ID n'existe pas.
 */
bool turnout_manager_set_position(int turnout_id, turnout_position_t position);

turnout_position_t turnout_manager_get_position(int id);

int turnout_manager_get_count(void);

#endif // TURNOUT_MANAGER_H