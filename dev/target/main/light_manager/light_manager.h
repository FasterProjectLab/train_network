#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void light_manager_init(void);

void light_manager_set(bool white_front, bool red_front, bool white_rear, bool red_rear);

void light_manager_update_by_current_dir(void);

bool light_manager_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif