#ifndef MAIN_CONFIG_H
#define MAIN_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_websocket_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdbool.h>

// ===== NETWORK CONFIGURATION =====
#define WIFI_SSID      "train_network"
#define WIFI_PASS      "password123"
/** * @brief WebSocket Server URL template. 
 * %s will be replaced by the device MAC address. 
 */
#define SERVER_WS_URL  "ws://192.168.10.1:8080/ws/%s?type=train"
#define FIRMWARE_URL   "https://192.168.10.1/firmware.bin"
// Easily configurable pins
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         47
#define I2C_PORT            0
#define I2C_FREQ_HZ         100000

/** @brief Global logging tag for the application */
static const char *TAG = "ESP32_TRAIN_APP";

// ===== SYSTEM & OTA =====
/** @brief Starts the Wi-Fi station mode */
void wifi_init_sta(void);

/** @brief Triggers an HTTPS OTA update from the specified URL */
void start_ota_update(const char* url);

// ===== CAMERA SERVICE =====
/** @brief Initializes the camera hardware */
esp_err_t camera_init_service(void);

/** @brief Starts or stops the camera data capture */
void camera_set_enabled(bool state);

/** @brief Initializes the camera internal task/queue */
void camera_task_init(void);

/** @brief Updates camera sensor parameters (brightness, contrast, etc.) */
void camera_service_update_settings(const char* variable, int value);

/** @brief Generates a unique SSRC ID based on the hardware MAC address */
uint32_t generate_ssrc_from_mac(void);

// ===== TELEMETRY SERVICE =====
/** @brief Initializes the periodic telemetry system */
esp_err_t telemetry_task_init(uint32_t interval_ms, esp_websocket_client_handle_t client);

/** @brief Enables or disables the telemetry heartbeat */
void telemetry_set_enabled(bool state, cJSON* payload);

// ===== MOTOR CONTROL SERVICE =====
/** @brief Configures PWM timers and GPIOs for motor control */
void motor_service_init(void);

/** @brief Sets motor speed (0-100%) and direction */
void motor_service_set_speed(uint8_t duty, bool forward);

/** @brief Immediate motor stop */
void motor_service_stop(void);

/** @brief Returns true if current set direction is forward */
bool motor_get_current_direction(void);

/**
 * @brief Sets the motor direction and updates associated services
 * @param forward True for forward, false for backward
 */
void motor_set_direction(bool forward);

// ===== LIGHTING SERVICE =====
/** @brief Configures GPIOs for the lighting system */
void light_service_init(void);

/** @brief Sets the state of front/rear white and red LEDs */
void light_service_set(bool white_front, bool red_front, bool white_rear, bool red_rear);

/** * @brief Returns the global lighting status.
 * @return True if at least one LED is powered ON, false if all LEDs are OFF.
 */
bool light_is_enabled();

// ===== WEBSOCKET APPLICATION =====
/** @brief Starts the WebSocket client task */
void websocket_app_start(void);

/** * @brief Sends a formatted JSON message over WebSocket
 * @param payload The cJSON object to send (function takes ownership and will delete it)
 */
void send_ws_envelope(esp_websocket_client_handle_t client, const char* type, const char* target, cJSON* payload);

/** * @brief Automatically updates lights based on the motor's current direction.
 * * Switches between front and rear lighting sets (e.g., white at the front, 
 * red at the rear) depending on whether the motor is in forward or reverse.
 */
void update_light_by_current_dir();

void i2c_service_init(void);
void i2c_service_scan(void);
esp_err_t i2c_service_write(uint8_t dev_addr, uint8_t *data, size_t len);
esp_err_t i2c_service_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len);
esp_err_t i2c_service_receive(uint8_t dev_addr, uint8_t *data, size_t len);

// Function prototypes
esp_err_t nfc_service_init(void);

#ifdef __cplusplus
}
#endif

#endif // MAIN_CONFIG_H

