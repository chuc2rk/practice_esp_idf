#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <bmp280.h>
#include <string.h>
#include <stdio.h>
#include "connect.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "driver/gpio.h"

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif
float pressure, temperature, humidity;

void bmp280_test(void *pvParameters)
{
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    bmp280_t dev;
    memset(&dev, 0, sizeof(bmp280_t));

    ESP_ERROR_CHECK(bmp280_init_desc(&dev, BMP280_I2C_ADDRESS_0, 0, GPIO_NUM_5, GPIO_NUM_6));
    ESP_ERROR_CHECK(bmp280_init(&dev, &params));

    bool bme280p = dev.id == BME280_CHIP_ID;
    printf("BMP280: found %s\n", bme280p ? "BME280" : "BMP280");

    

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (bmp280_read_float(&dev, &temperature, &pressure, &humidity) != ESP_OK)
        {
            printf("Temperature/pressure reading failed\n");
            continue;
        }

        /* float is used in printf(). you need non-default configuration in
         * sdkconfig for ESP8266, which is enabled by default for this
         * example. see sdkconfig.defaults.esp8266
         */
        printf("Pressure: %.2f Pa, Temperature: %.2f C", pressure, temperature);
        if (bme280p)
            printf(", Humidity: %.2f\n", humidity);
        else
            printf("\n");
    }
}


esp_err_t get_handler(httpd_req_t *req)
{
    char resp_str[512];
    snprintf(resp_str, sizeof(resp_str),
             "<!DOCTYPE html>"
             "<html>"
             "<body>"
             "<h1>ESP32 Weather Station</h1>"
             "<p>Temperature: %.1f C</p>"
             "<p>Pressure: %.2f hPa</p>"
             "</body>"
             "</html>", temperature, pressure / 100.0);

    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    //ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        //ESP_LOGI(TAG, "Registering URI handlers");

        httpd_uri_t uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri);
    }
    if (server == NULL) {
        //ESP_LOGE(TAG, "Failed to start the server");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta("ssid", "password", 10000));

    start_webserver();
    ESP_ERROR_CHECK(i2cdev_init());
    xTaskCreatePinnedToCore(bmp280_test, "bmp280_test", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM);    
}


