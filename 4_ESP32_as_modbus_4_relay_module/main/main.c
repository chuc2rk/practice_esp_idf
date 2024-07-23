#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "modbus_4relay.h"

#define RELAY1_GPIO GPIO_NUM_3
#define RELAY2_GPIO GPIO_NUM_4
#define RELAY3_GPIO GPIO_NUM_5
#define RELAY4_GPIO GPIO_NUM_6

void gpio_init() {
    gpio_reset_pin(RELAY1_GPIO);
    gpio_set_direction(RELAY1_GPIO, GPIO_MODE_OUTPUT);

    gpio_reset_pin(RELAY2_GPIO);
    gpio_set_direction(RELAY2_GPIO, GPIO_MODE_OUTPUT);

    gpio_reset_pin(RELAY3_GPIO);
    gpio_set_direction(RELAY3_GPIO, GPIO_MODE_OUTPUT);

    gpio_reset_pin(RELAY4_GPIO);
    gpio_set_direction(RELAY4_GPIO, GPIO_MODE_OUTPUT);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_init();
    uart_init();

    xTaskCreate(modbus_task, "modbus_task", 2048, NULL, 10, NULL);
}
