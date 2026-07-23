#include "camera_manager.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "main_config.h"
#include "lwip/sockets.h"
#include <string.h>
#include "esp_mac.h"
#include "rom/ets_sys.h" 

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SSCB_SDA 4
#define CAM_PIN_SSCB_SCL 5
#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

#define DEST_IP "192.168.10.1"
#define DEST_PORT 5004
#define MAX_RTP_PAYLOAD 1400
#define QUALITY_MIN 8    // Best quality (sharp image)
#define QUALITY_MAX 45   // Lowest quality (high compression)
#define TARGET_SEND_TIME_MS 35 // Threshold for network congestion

static const char *TAG = "CAMERA_MANAGER";

static TaskHandle_t s_stream_task_handle = NULL;
static bool s_camera_active = false;

typedef struct {
    int sock;
    struct sockaddr_in dest_addr;
    uint32_t ssrc;
    uint16_t seq_num;
    uint32_t timestamp;
    int quality;
} stream_ctx_t;

static void stream_task(void *pvParameters);
static bool send_rtp_packet(stream_ctx_t *ctx, camera_fb_t *fb, size_t offset, size_t chunk_size, bool is_last);
static void process_and_send_fb(stream_ctx_t *ctx, camera_fb_t *fb);
static int stream_init_socket(struct sockaddr_in *dest_addr);

uint32_t camera_manager_generate_ssrc_from_mac(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Simple hash to derive a 32-bit SSRC from the 48-bit MAC
    uint32_t ssrc = ((uint32_t)(mac[0] ^ mac[4]) << 24) |
                    ((uint32_t)(mac[1] ^ mac[5]) << 16) |
                    ((uint32_t)mac[2] << 8) |
                    ((uint32_t)mac[3]);
    return ssrc;
}

/**
 * @brief Initialise le matériel de la caméra
 */
esp_err_t camera_manager_init_hardware(void) {
    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sscb_sda = CAM_PIN_SSCB_SDA,
        .pin_sscb_scl = CAM_PIN_SSCB_SCL,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 10,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST // Reduce latency by skipping old frames
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }

    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
        // Correct orientation for typical ESP32-S3-CAM mounts
        s->set_hmirror(s, 1); 
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_contrast(s, 1);
    }
    
    return ESP_OK;
}

void camera_manager_set_enabled(bool state) {
    s_camera_active = state;
    ESP_LOGI(TAG, "Camera streaming status: %s", state ? "RESUMED" : "PAUSED");

    if (state && s_stream_task_handle != NULL) {
        xTaskNotifyGive(s_stream_task_handle);
    }
}

bool camera_manager_is_active(void) {
    return s_camera_active;
}

void camera_manager_task_init(void) {
    xTaskCreatePinnedToCore(stream_task, "camera_stream_task", 4096, NULL, 4, &s_stream_task_handle, 1);
}

void camera_manager_update_settings(const char* variable, int value) {
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) return;

    if (strcmp(variable, "framesize") == 0) {
        s->set_framesize(s, (framesize_t)value);
        ESP_LOGI(TAG, "Resolution updated: %d", value);
    } 
    else if (strcmp(variable, "quality") == 0) {
        s->set_quality(s, value);
        ESP_LOGI(TAG, "JPEG quality updated: %d", value);
    }
}

/**
 * @brief Constructs and sends an RTP packet with JPEG fragment
 */
static bool send_rtp_packet(stream_ctx_t *ctx, camera_fb_t *fb, size_t offset, size_t chunk_size, bool is_last) {
    uint8_t rtp_header[12];
    rtp_header[0] = 0x80; // Version 2
    rtp_header[1] = 0x1a | (is_last ? 0x80 : 0x00); // Payload type 26 (JPEG) + Marker bit

    // Network byte order conversion
    uint16_t n_seq = htons(ctx->seq_num++);
    uint32_t n_ts = htonl(ctx->timestamp);
    uint32_t n_ssrc = htonl(ctx->ssrc);

    memcpy(&rtp_header[2], &n_seq, 2);
    memcpy(&rtp_header[4], &n_ts, 4);
    memcpy(&rtp_header[8], &n_ssrc, 4);

    // RFC 2435 JPEG Header
    uint8_t jpg_header[8] = {
        0x00,                               // Specific
        (uint8_t)((offset >> 16) & 0xFF),   // Fragment Offset (24-bit)
        (uint8_t)((offset >> 8) & 0xFF),
        (uint8_t)(offset & 0xFF),
        0x01,                               // Type 1 (JPEG)
        (uint8_t)ctx->quality,              // Precision/Quality
        (uint8_t)(fb->width / 8),           // Width in 8-pixel blocks
        (uint8_t)(fb->height / 8)           // Height in 8-pixel blocks
    };

    struct iovec iov[3] = {
        { .iov_base = rtp_header, .iov_len = 12 },
        { .iov_base = jpg_header, .iov_len = 8 },
        { .iov_base = fb->buf + offset, .iov_len = chunk_size }
    };

    struct msghdr msg = {
        .msg_name = &ctx->dest_addr,
        .msg_namelen = sizeof(ctx->dest_addr),
        .msg_iov = iov,
        .msg_iovlen = 3
    };

    return sendmsg(ctx->sock, &msg, 0) >= 0;
}

static void process_and_send_fb(stream_ctx_t *ctx, camera_fb_t *fb) {
    size_t sent_len = 0;
    int consecutive_errors = 0;

    while (sent_len < fb->len) {
        size_t remaining = fb->len - sent_len;
        size_t chunk_size = (remaining > 1380) ? 1380 : remaining;
        bool is_last = (sent_len + chunk_size >= fb->len);

        if (send_rtp_packet(ctx, fb, sent_len, chunk_size, is_last)) {
            sent_len += chunk_size;
            consecutive_errors = 0;
            ets_delay_us(150); 
        } else {
            consecutive_errors++;
            vTaskDelay(pdMS_TO_TICKS(2)); 

            if (consecutive_errors > 50) {
                ESP_LOGW(TAG, "Network congestion: frame dropped");
                break;
            }
        }
    }
}

static int stream_init_socket(struct sockaddr_in *dest_addr) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket creation failed");
        return -1;
    }

    dest_addr->sin_addr.s_addr = inet_addr(DEST_IP);
    dest_addr->sin_family = AF_INET;
    dest_addr->sin_port = htons(DEST_PORT);
    return sock;
}

static void stream_task(void *pvParameters) {
    stream_ctx_t ctx = {
        .ssrc = camera_manager_generate_ssrc_from_mac(),
        .seq_num = 0,
        .timestamp = 0,
    };

    ctx.sock = stream_init_socket(&ctx.dest_addr);
    if (ctx.sock < 0) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Stream task started and ready");

    while (1) {
        if (!s_camera_active) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        process_and_send_fb(&ctx, fb);

        ctx.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
        esp_camera_fb_return(fb);
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}