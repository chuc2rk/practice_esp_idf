#include <stdio.h>
#include "connect.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip.h"


#define BLINK_GPIO 8
#define BLINK_LED_RMT_CHANNEL 0

static const char* TAG = "SERVER";
static uint8_t s_led_state = 0;
static led_strip_t *pStrip_a;

static void configure_led(void)
{
  ESP_LOGI(TAG, "Configuring LED");
  pStrip_a = led_strip_init(BLINK_LED_RMT_CHANNEL, BLINK_GPIO, 1);
  pStrip_a->clear(pStrip_a, 50);
}

static void set_led_state(uint8_t state)
{
  ESP_LOGI(TAG, "Setting LED state: %d", state);
  if (state)
  {
    pStrip_a->set_pixel(pStrip_a, 0, 255, 0, 0);
    pStrip_a->refresh(pStrip_a, 100);
  }
  else
  {
    pStrip_a->clear(pStrip_a, 50);
  }
}



static esp_err_t uri_default_handler(httpd_req_t *req)
{
  const char* resp_str =  "<!DOCTYPE html>"
                          "<html>"
                          "<body>"
                          "<h1>ESP32 LED control</h1>"
                          "<p>LED state: "
                          "<span id=\"led_state\">OFF</span>"                          
                          "</p>"
                          "<button onclick=\"location.href='/on'\" type=\"button\">ON</button>"
                          "<button onclick=\"location.href='/off'\" type=\"button\">OFF</button>"
                          "<script>"
                          "function updateLedState() {"
                          " fetch('/status').then(response => response.text()).then(state => {"
                          "   document.getElementById('led_state').innerText = state;"
                          "   });"
                          "}"
                          "setInterval(updateLedState, 1000);"
                          "</script>"
                          "</body>"
                          "</html>";
  httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t led_on_handler(httpd_req_t *req)
{
  s_led_state = 1;
  set_led_state(s_led_state);  
  httpd_resp_send(req, "LED is ON", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t led_off_handler(httpd_req_t *req)
{
  s_led_state = 0;
  set_led_state(s_led_state);
  httpd_resp_send(req, "LED is OFF", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t led_status_handler(httpd_req_t *req)
{
  const char *state_str = s_led_state ? "ON" : "OFF";
  httpd_resp_send(req, state_str, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static void start_webserver(void)
{
  // Start the httpd server
  httpd_handle_t server = NULL;
  // Configure the server 
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

  if (httpd_start(&server, &config) == ESP_OK) {
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_uri_t uri_default = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = uri_default_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_default);

    httpd_uri_t led_on = {
      .uri = "/on",
      .method = HTTP_GET,
      .handler = led_on_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &led_on);

    httpd_uri_t led_off = {
      .uri = "/off",
      .method = HTTP_GET,
      .handler = led_off_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &led_off);

    httpd_uri_t led_status = {
      .uri = "/status",
      .method = HTTP_GET,
      .handler = led_status_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &led_status);

    if (server == NULL) {
      ESP_LOGE(TAG, "Error starting server!");
      return;
    }
  }
}

void app_main(void)
{
  ESP_ERROR_CHECK(nvs_flash_init());
  wifi_init();  
  ESP_ERROR_CHECK(wifi_connect_sta("yourssid", "yourpass", 10000));
  
  configure_led();
  start_webserver();
}
