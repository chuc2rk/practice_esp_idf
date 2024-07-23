#include "driver/uart.h"
#include "esp_log.h"
#include "modbus_4relay.h"
#include "driver/gpio.h"

#define UART_NUM UART_NUM_1
#define TXD_PIN (GPIO_NUM_8)
#define RXD_PIN (GPIO_NUM_9)
#define RTS_PIN (GPIO_NUM_10)
#define BAUD_RATE 9600

static const char *TAG = "MODBUS_MASTER";

void uart_init() {
    const uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_driver_install(UART_NUM, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, RTS_PIN, UART_PIN_NO_CHANGE);
}

void modbus_send_response(uint8_t address, uint8_t function, uint16_t reg, uint16_t value) {
    uint8_t response[8];
    uint16_t crc;

    response[0] = address;
    response[1] = function;
    response[2] = (reg >> 8) & 0xFF;
    response[3] = reg & 0xFF;
    response[4] = (value >> 8) & 0xFF;
    response[5] = value & 0xFF;

    crc = calculate_crc(response, 6);
    response[6] = crc & 0xFF;
    response[7] = (crc >> 8) & 0xFF;

    uart_write_bytes(UART_NUM, (const char *)response, 8);
}

uint16_t calculate_crc(uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void handle_modbus_command(uint8_t *data, uint16_t length) {
    if (length < 8) {
        ESP_LOGE(TAG, "Invalid Modbus frame");
        return;
    }

    uint8_t address = data[0];
    uint8_t function = data[1];
    uint16_t reg = (data[2] << 8) | data[3];
    uint16_t value = (data[4] << 8) | data[5];
    uint16_t crc_received = (data[6] << 8) | data[7];
    uint16_t crc_calculated = calculate_crc(data, 6);

    if (crc_received != crc_calculated) {
        ESP_LOGE(TAG, "CRC mismatch");
        return;
    }

    if (address == 1) { // Check if the address matches
        if (function == 0x05) {
            uint8_t relay = reg & 0x000F;
            gpio_set_level(relay, value ? 1 : 0);
            modbus_send_response(address, function, reg, value);
        }
    }
}

void modbus_task(void *arg) {
    uint8_t data[256];
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, sizeof(data), 100 / portTICK_RATE_MS);
        if (len > 0) {
            handle_modbus_command(data, len);
        }
    }
}