#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "connect.h"
#include "modbus.h"

static const char *TAG = "WEBSERVER";

esp_err_t relay_on_handler(httpd_req_t *req, uint8_t relay) {
    relay_control(relay, 1);
    httpd_resp_send(req, "Relay is ON", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t relay_off_handler(httpd_req_t *req, uint8_t relay) {
    relay_control(relay, 0);
    httpd_resp_send(req, "Relay is OFF", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t relay1_on_handler(httpd_req_t *req) { return relay_on_handler(req, 0); }
esp_err_t relay1_off_handler(httpd_req_t *req) { return relay_off_handler(req, 0); }
esp_err_t relay2_on_handler(httpd_req_t *req) { return relay_on_handler(req, 1); }
esp_err_t relay2_off_handler(httpd_req_t *req) { return relay_off_handler(req, 1); }
esp_err_t relay3_on_handler(httpd_req_t *req) { return relay_on_handler(req, 2); }
esp_err_t relay3_off_handler(httpd_req_t *req) { return relay_off_handler(req, 2); }
esp_err_t relay4_on_handler(httpd_req_t *req) { return relay_on_handler(req, 3); }
esp_err_t relay4_off_handler(httpd_req_t *req) { return relay_off_handler(req, 3); }

esp_err_t get_handler(httpd_req_t *req) {
    const char *resp_str = "<!DOCTYPE html>"
                           "<html>"
                           "<head>"
                           "<style>"
                           "body { font-family: Arial, sans-serif; }"
                           "h1 { text-align: center; }"
                           ".relay { display: flex; justify-content: space-around; margin: 20px; }"
                           ".button { padding: 10px 20px; font-size: 16px; }"
                           "</style>"
                           "</head>"
                           "<body>"
                           "<h1>ESP32 Modbus Relay Control</h1>"
                           "<div class='relay'>"
                           "<div>"
                           "<h2>Relay 1</h2>"
                           "<button class='button' onclick=\"location.href='/relay1/on'\" type=\"button\">ON</button>"
                           "<button class='button' onclick=\"location.href='/relay1/off'\" type=\"button\">OFF</button>"
                           "</div>"
                           "<div>"
                           "<h2>Relay 2</h2>"
                           "<button class='button' onclick=\"location.href='/relay2/on'\" type=\"button\">ON</button>"
                           "<button class='button' onclick=\"location.href='/relay2/off'\" type=\"button\">OFF</button>"
                           "</div>"
                           "<div>"
                           "<h2>Relay 3</h2>"
                           "<button class='button' onclick=\"location.href='/relay3/on'\" type=\"button\">ON</button>"
                           "<button class='button' onclick=\"location.href='/relay3/off'\" type=\"button\">OFF</button>"
                           "</div>"
                           "<div>"
                           "<h2>Relay 4</h2>"
                           "<button class='button' onclick=\"location.href='/relay4/on'\" type=\"button\">ON</button>"
                           "<button class='button' onclick=\"location.href='/relay4/off'\" type=\"button\">OFF</button>"
                           "</div>"
                           "</div>"
                           "</body>"
                           "</html>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");


        httpd_uri_t uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri);

        httpd_uri_t relay1_on = {
            .uri      = "/relay1/on",
            .method   = HTTP_GET,
            .handler  = relay1_on_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relay1_on);

        httpd_uri_t relay1_off = {
            .uri      = "/relay1/off",
            .method   = HTTP_GET,
            .handler  = relay1_off_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relay1_off);

        httpd_uri_t relay2_on = {
            .uri      = "/relay2/on",
            .method   = HTTP_GET,
            .handler  = relay2_on_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relay2_on);

        httpd_uri_t relay2_off = {
            .uri      = "/relay2/off",
            .method   = HTTP_GET,
            .handler  = relay2_off_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relay2_off);

        httpd_uri_t relay3_on = {
            .uri      = "/relay3/on",
            .method   = HTTP_GET,
            .handler  = relay3_on_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relay3_on);

        httpd_uri_t relay3_off = {
            .uri      = "/relay3/off",
            .method   = HTTP_GET,
            .handler  = relay3_off_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relay3_off);

        httpd_uri_t relay4_on = {
            .uri      = "/relay4/on",
            .method   = HTTP_GET,
            .handler  = relay4_on_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relay4_on);

        httpd_uri_t relay4_off = {
            .uri      = "/relay4/off",
            .method   = HTTP_GET,
            .handler  = relay4_off_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relay4_off);
    }
    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to start the server");
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta("yourssid", "yourpass", 10000));

    uart_init();
    start_webserver();
}