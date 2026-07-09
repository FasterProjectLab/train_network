#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void start_ota_update(const char* url);

#ifdef __cplusplus
}
#endif

#endif 