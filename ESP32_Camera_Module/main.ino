#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ==========================================
// 1. NETWORK SETTINGS
// ==========================================
// This MUST match the hotspot the ESP8266 is broadcasting
const char* ssid = "Rover_Network";
const char* password = "robotic";

// ==========================================
// 2. HARDWARE PINS (AI-Thinker Model)
// ==========================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_LED_PIN      4  // The super bright onboard LED

// Web server variable
httpd_handle_t camera_httpd = NULL;

// ==========================================
// 3. SERVER ENDPOINTS (Video & Flashlight)
// ==========================================

// Turns Flash ON
static esp_err_t light_on_handler(httpd_req_t *req) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    httpd_resp_send(req, "Flashlight ON", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Turns Flash OFF
static esp_err_t light_off_handler(httpd_req_t *req) {
    digitalWrite(FLASH_LED_PIN, LOW);
    httpd_resp_send(req, "Flashlight OFF", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// The MJPEG Video Stream Handler
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;

        size_t hlen = snprintf((char *)part_buf, 64, "\r\n--123456789000000000000987654321\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, "\r\n", 2);
        }
        
        esp_camera_fb_return(fb);
        if (res != ESP_OK) break;
    }
    return res;
}

// Start the server and register URLs
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
    httpd_uri_t light_on_uri = { .uri = "/lighton", .method = HTTP_GET, .handler = light_on_handler, .user_ctx = NULL };
    httpd_uri_t light_off_uri = { .uri = "/lightoff", .method = HTTP_GET, .handler = light_off_handler, .user_ctx = NULL };

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &stream_uri);
        httpd_register_uri_handler(camera_httpd, &light_on_uri);
        httpd_register_uri_handler(camera_httpd, &light_off_uri);
    }
}

// ==========================================
// 4. MAIN SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW); // Start with flash off

    // Configure Camera
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    
    // Low resolution for high-speed AI data collection (VGA = 640x480)
    config.frame_size = FRAMESIZE_VGA; 
    config.jpeg_quality = 12; // 0-63 lower number means higher quality
    config.fb_count = 1;

    // Initialize Camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }

    // Connect to ESP8266 WiFi Hotspot
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    
    // Boot Server
    startCameraServer();
    
    Serial.print("Camera Ready! Connect to: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/stream");
}

void loop() {
    // The web server runs asynchronously in the background.
    // The main loop does nothing to save power.
    delay(10000); 
}
