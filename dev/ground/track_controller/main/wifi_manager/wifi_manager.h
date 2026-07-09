#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

/**
 * @brief Initialise le Wi-Fi en mode Station et lance la boucle de reconnexion automatique.
 */
void wifi_manager_init(void);

/**
 * @brief Vérifie si le Wi-Fi est actuellement connecté et dispose d'une IP.
 * @return true si connecté, false sinon.
 */
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H