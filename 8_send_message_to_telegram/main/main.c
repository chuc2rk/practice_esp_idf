#include <stdio.h>
#include "connect.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sntp.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define DOOR_SENSOR_PIN GPIO_NUM_4
#define DEBOUNCE_TIME   50
static const char* TAG = "DOOR_MONITOR";

extern const unsigned char telegram[] asm("_binary_telegram_crt_start");

// Time zone settings
#define TIME_ZONE "ICT-7"  // Bangkok timezone (UTC+7)

// Initialize and sync time via NTP
void init_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    // Wait for time to be sync'd
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    // Set timezone
    setenv("TZ", TIME_ZONE, 1);
    tzset();
}

void send_telegram_message(const char* message) {
    char post_data[512];
    snprintf(post_data, sizeof(post_data), 
        "{\"chat_id\": -45??????, \"text\": \"%s\"}", 
        message);
    
    esp_http_client_config_t esp_http_client_config = {    
        .method = HTTP_METHOD_POST,
        .host = "api.telegram.org",
        .port = 443,
        .path = "/bot:token/sendMessage",
        .timeout_ms = 60000,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = (char*)telegram,
        .use_global_ca_store = false,
        .skip_cert_common_name_check = false,
        .is_async = false,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&esp_http_client_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {    
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
        
        if (status_code == 200) {
            ESP_LOGI(TAG, "Message sent successfully");
        } else {
            ESP_LOGE(TAG, "Message send failed with status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
}

void init_door_sensor() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DOOR_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void door_monitor_task(void *pvParameters) {
    int last_state = gpio_get_level(DOOR_SENSOR_PIN);
    char time_str[64];
    time_t now;
    struct tm timeinfo;
    
    while(1) {
        int current_state = gpio_get_level(DOOR_SENSOR_PIN);
        
        if(current_state != last_state) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME));
            current_state = gpio_get_level(DOOR_SENSOR_PIN);
            
            if(current_state != last_state) {
                time(&now);
                localtime_r(&now, &timeinfo);
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
                
                char message[256];
                if(current_state == 0) {
                    snprintf(message, sizeof(message), "🚪 May quá nhà không có gì nên nó đóng cửa lại rồi, lúc %s", time_str);
                } else {
                    snprintf(message, sizeof(message), "⚠️ Trộm vừa mở xem có gì trong nhà không kìa, lúc %s", time_str);
                }
                
                send_telegram_message(message);
                last_state = current_state;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialize WiFi
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta("wifi", "wifipass", 10000));
    
    // Initialize and sync time with NTP server
    init_sntp();
    
    // Initialize door sensor
    init_door_sensor();
    
    // Get current time after sync
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // Send initial status message with current time
    char init_message[256];
    snprintf(init_message, sizeof(init_message), 
             "🔄 Door monitoring system started at %s!", strftime_buf);
    send_telegram_message(init_message);
    
    // Create door monitoring task
    xTaskCreate(door_monitor_task, "door_monitor_task", 4096, NULL, 5, NULL);
}