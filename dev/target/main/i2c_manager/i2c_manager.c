#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "main_config.h"

#define I2C_SDA_PIN         47
#define I2C_SCL_PIN         21
#define I2C_PORT            0
#define I2C_FREQ_HZ         100000

// Tag unique pour les logs de ce module
static const char *TAG = "I2C_MANAGER";

static i2c_master_bus_handle_t bus_handle;

void i2c_manager_init(void) {
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, // Note: External resistors recommended for stability
    };

    // Initialize the I2C bus as master
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
    ESP_LOGI(TAG, "I2C bus initialized on SDA:%d SCL:%d", I2C_SDA_PIN, I2C_SCL_PIN);
}

void i2c_manager_scan(void) {
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int devices_found = 0;

    for (int i = 3; i < 127; i++) {
        // Probe checks if a device acknowledges the address
        esp_err_t ret = i2c_master_probe(bus_handle, i, 100);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at address: 0x%02X", i);
            devices_found++;
        }
    }

    if (devices_found == 0) {
        ESP_LOGW(TAG, "No I2C devices found.");
    } else {
        ESP_LOGI(TAG, "Scan complete. %d devices found.", devices_found);
    }
}

esp_err_t i2c_manager_write(uint8_t dev_addr, uint8_t *data, size_t len) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    
    i2c_master_dev_handle_t dev_handle;
    // Temporarily add device to the bus
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    // Perform the write operation
    esp_err_t ret = i2c_master_transmit(dev_handle, data, len, 1000);
    
    // Cleanup handle to free memory
    i2c_master_bus_rm_device(dev_handle);
    return ret;
}

esp_err_t i2c_manager_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

    // Write register address then read 'len' bytes from it
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, 1000);

    i2c_master_bus_rm_device(dev_handle);
    return ret;
}

esp_err_t i2c_manager_receive(uint8_t dev_addr, uint8_t *data, size_t len) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    esp_err_t ret = i2c_master_receive(dev_handle, data, len, 1000);

    i2c_master_bus_rm_device(dev_handle);
    return ret;
}