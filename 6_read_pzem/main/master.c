// Copyright 2016-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "string.h"
#include "esp_log.h"
#include "mbcontroller.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "connect.h"


#define MB_PORT_NUM      UART_NUM_1   // Number of UART port used for Modbus connection
#define MB_DEV_SPEED    9600  // The communication speed of the UART
#define MB_UART_TXD 17
#define MB_UART_RXD 16
#define MB_DEVICE_ADDR  1 

static const char *TAG = "PZEM";

// Global variables to store the latest readings
float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;
float frequency = 0;
float power_factor = 0;

// Function prototypes
static esp_err_t master_init(void);
static esp_err_t root_get_handler(httpd_req_t *req);
 
// Modbus master initialization
static esp_err_t master_init(void)
{
    // Initialize and start Modbus controller
    mb_communication_info_t comm = {
            .port = MB_PORT_NUM,
            .mode = MB_MODE_RTU,
            .baudrate = MB_DEV_SPEED,
            .parity = MB_PARITY_NONE
    };
    void* master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG,
                                "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller initialization fail, returns(0x%x).",
                            (uint32_t)err);
    err = mbc_master_setup((void*)&comm);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller setup fail, returns(0x%x).",
                            (uint32_t)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, MB_UART_TXD, MB_UART_RXD,
                              UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
            "mb serial set pin failure, uart_set_pin() returned (0x%x).", (uint32_t)err);

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller start fail, returns(0x%x).",
                            (uint32_t)err);

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
            "mb serial set mode failure, uart_set_mode() returned (0x%x).", (uint32_t)err);

    vTaskDelay(5);
    
    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}

// 
void read_pzem_data(void *pvParameters)
{
  
    int16_t value[10];

    mb_param_request_t request_cfg = {
      .slave_addr = 0x01,     /*!< Modbus slave address */
      .command    = 0x04,     /*!< Modbus command to send */
      .reg_start  = 0x0000,   /*!< Modbus start register */
      .reg_size   = 0x000A    /*!< Modbus number of registers */
    };

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
        // send request
        esp_err_t err = mbc_master_send_request(&request_cfg, &value);

        if (err == ESP_OK) {
              voltage = value[0] / 10.0f;
              current = ((uint32_t)value[2] << 16 | value[1]) / 1000.0f;
              power = ((uint32_t)value[4] << 16 | value[3]) / 10.0f;
              energy = ((uint32_t)value[6] << 16 | value[5]) / 1000.0f;
              frequency = value[7] / 10.0f;
              power_factor = value[8] / 100.0f;

              ESP_LOGI(TAG, "Voltage: %.1f V", voltage);
              ESP_LOGI(TAG, "Current: %.3f A", current);
              ESP_LOGI(TAG, "Power: %.1f W", power);
              ESP_LOGI(TAG, "Energy: %.3f kWh", energy);
              ESP_LOGI(TAG, "Frequency: %.1f Hz", frequency);
              ESP_LOGI(TAG, "Power Factor: %.2f", power_factor);
        } else {
            ESP_LOGE(TAG, "Error reading PZEM data: %s", esp_err_to_name(err));
        }
    }         
}


// HTTP GET handler for root path
static esp_err_t root_get_handler(httpd_req_t *req)
{
    char response[1024];

    snprintf(response, sizeof(response),
             "<html><head><title>PZEM Data</title>"
             "<meta http-equiv='refresh' content='5'>"
             "<style>body{font-family: Arial, sans-serif;}"
             "table{border-collapse: collapse;}"
             "th, td{border: 1px solid black; padding: 5px;}"
             "</style></head>"
             "<body><h1>PZEM Data</h1>"
             "<table>"
             "<tr><th>Measurement</th><th>Value</th></tr>"
             "<tr><td>Voltage</td><td>%.1f V</td></tr>"
             "<tr><td>Current</td><td>%.3f A</td></tr>"
             "<tr><td>Power</td><td>%.1f W</td></tr>"
             "<tr><td>Energy</td><td>%.3f kWh</td></tr>"
             "<tr><td>Frequency</td><td>%.1f Hz</td></tr>"
             "<tr><td>Power Factor</td><td>%.2f</td></tr>"
             "</table></body></html>",
             voltage, current, power, energy, frequency, power_factor);
    
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// HTTP server configuration
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root);
    }

    return server;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta("wifi", "password", 10000));

    // Initialization of device peripheral and objects
    ESP_ERROR_CHECK(master_init());

    // Start the web server
    start_webserver();

    xTaskCreate(read_pzem_data, "pzem_task", 4096, NULL, 5, NULL);
}

