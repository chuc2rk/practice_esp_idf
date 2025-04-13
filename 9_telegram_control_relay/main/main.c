#include <stdio.h>
#include "connect.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define LED_PIN GPIO_NUM_4  // Changed from RELAY_PIN to LED_PIN and using built-in LED pin
#define MAX_HTTP_RECV_BUFFER 512
static const char* TAG = "LED_CONTROL";  // Updated tag
static int led_state = 0;

// Telegram Bot information
#define TELEGRAM_BOT_TOKEN "your token here"  // Replace with your actual bot token
#define TELEGRAM_CHAT_ID "-your chat id here"  // Replace with your actual chat ID
#define TELEGRAM_API_HOST "api.telegram.org"

// Last update ID to avoid processing the same message multiple times
long last_update_id = 0;

extern const unsigned char telegram[] asm("_binary_telegram_crt_start");

void init_led() {  // Renamed from init_relay to init_led
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Initialize LED to OFF state
    gpio_set_level(LED_PIN, 0);
    ESP_LOGI(TAG, "LED initialized to OFF state");
}

void send_telegram_message(const char* message) {
    char post_data[512];
    snprintf(post_data, sizeof(post_data), 
        "{\"chat_id\": %s, \"text\": \"%s\"}", 
        TELEGRAM_CHAT_ID, message);
    
    esp_http_client_config_t esp_http_client_config = {    
        .method = HTTP_METHOD_POST,
        .host = TELEGRAM_API_HOST,
        .port = 443,
        .path = "/bot" TELEGRAM_BOT_TOKEN "/sendMessage",
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

// HTTP event handler to process Telegram API responses
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    static char *output_buffer;  // Buffer to store response
    static int output_len;       // Response buffer length

    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // If output buffer is NULL, allocate
            if (!output_buffer) {
                output_len = 0;
                output_buffer = (char *)malloc(MAX_HTTP_RECV_BUFFER);
                if (!output_buffer) {
                    ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                    return ESP_FAIL;
                }
                output_buffer[0] = '\0';
            }
            
            // Copy received data to buffer
            if (output_len + evt->data_len < MAX_HTTP_RECV_BUFFER) {
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
                output_buffer[output_len] = '\0';
            } else {
                ESP_LOGW(TAG, "Output buffer full, discarding data");
            }
            break;
            
        case HTTP_EVENT_ON_FINISH:
            if (output_buffer != NULL) {
                // Process response
                if (strstr(output_buffer, "\"ok\":true") != NULL) {
                    // Check if we have updates
                    if (strstr(output_buffer, "\"update_id\"") != NULL) {
                        char *text_start = strstr(output_buffer, "\"text\":\"");
                        char *update_id_start = strstr(output_buffer, "\"update_id\":");
                        long update_id = 0;
                        
                        if (update_id_start) {
                            // Parse update_id
                            sscanf(update_id_start, "\"update_id\":%ld", &update_id);
                            
                            // Process only if we haven't seen this update before
                            if (update_id > last_update_id) {
                                last_update_id = update_id;
                                
                                // Extract command text
                                if (text_start) {
                                    text_start += 8; // Move past "\"text\":\""
                                    char command[64] = {0};
                                    int i = 0;
                                    while (*text_start != '\"' && i < 63) {
                                        command[i++] = *text_start++;
                                    }
                                    command[i] = '\0';
                                    
                                    ESP_LOGI(TAG, "Received command: %s", command);
                                    
                                    // Process commands
                                    if (strcmp(command, "/on") == 0 || strcasecmp(command, "bật") == 0) {
                                        gpio_set_level(LED_PIN, 1);
                                        led_state = 1;
                                        send_telegram_message("✅ Đã bật đèn!");
                                    } 
                                    else if (strcmp(command, "/off") == 0 || strcasecmp(command, "tắt") == 0) {
                                        gpio_set_level(LED_PIN, 0);
                                        led_state = 0;
                                        send_telegram_message("❌ Đã tắt đèn!");
                                    }
                                    else if (strcmp(command, "/status") == 0 || strcasecmp(command, "trạng thái") == 0) {
                                        // Check LED state and send status message
                                        if (led_state) {
                                            send_telegram_message("💡 Đèn đang BẬT");
                                        } else {
                                            send_telegram_message("⚫ Đèn đang TẮT");
                                        }
                                    } else if (strcmp(command, "/help") == 0 || strcasecmp(command, "trợ giúp") == 0) {
                                        send_telegram_message("📜 Danh sách lệnh:\n"
                                                             "/on - Bật đèn\n"
                                                             "/off - Tắt đèn\n"
                                                             "/status - Kiểm tra trạng thái đèn\n"
                                                             "/help - Hiển thị danh sách lệnh");
                                    } else {
                                        send_telegram_message("❓ Lệnh không hợp lệ. Vui lòng thử lại!");
                                    }
                                }
                            }
                        }
                    }
                }
                // Clear buffer
                free(output_buffer);
                output_buffer = NULL;
                output_len = 0;
            }
            break;
            
        case HTTP_EVENT_DISCONNECTED:
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
                output_len = 0;
            }
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

void check_telegram_updates_task(void *pvParameters) {
    esp_http_client_config_t config = {
        .host = TELEGRAM_API_HOST,
        .path = "/bot" TELEGRAM_BOT_TOKEN "/getUpdates",
        .port = 443,
        .method = HTTP_METHOD_GET,  // Explicitly set the method to GET
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = (char*)telegram,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };
    
    char url_query[128];
    
    while(1) {
        // Update the query to include offset (last update ID + 1)
        snprintf(url_query, sizeof(url_query), "/bot%s/getUpdates?offset=%ld&timeout=5", 
                 TELEGRAM_BOT_TOKEN, last_update_id + 1);
        
        config.path = url_query;
        esp_http_client_handle_t client = esp_http_client_init(&config);
        
        if (client != NULL) {
            esp_err_t err = esp_http_client_perform(client);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get Telegram updates: %s", esp_err_to_name(err));
            }
            esp_http_client_cleanup(client);
        }
        
        // Wait for 2 seconds before checking again
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialize WiFi
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta("your_wifi_ssid", "your_wifi_password", 10000));
    
    // Initialize LED
    init_led();
    
    // Send initial status message
    send_telegram_message("🔄 Hệ thống điều khiển đèn đã khởi động!");
    
    // Create task to check for Telegram updates
    xTaskCreate(check_telegram_updates_task, "telegram_updates_task", 8192, NULL, 5, NULL);
}