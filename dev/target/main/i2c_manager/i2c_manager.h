#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

void i2c_manager_init(void);
void i2c_manager_scan(void);
esp_err_t i2c_manager_write(uint8_t dev_addr, uint8_t *data, size_t len);
esp_err_t i2c_manager_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len);
esp_err_t i2c_manager_receive(uint8_t dev_addr, uint8_t *data, size_t len);

#endif // I2C_MANAGER_H