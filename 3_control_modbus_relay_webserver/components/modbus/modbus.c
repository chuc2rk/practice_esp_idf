#include "driver/uart.h"
#include "esp_log.h"
#include "modbus.h"
#include "driver/gpio.h"

#define UART_NUM UART_NUM_1
#define TXD_PIN GPIO_NUM_6
#define RXD_PIN GPIO_NUM_7
#define RTS_PIN GPIO_NUM_8
#define BAUD_RATE 9600


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

void modbus_send_command(uint8_t address, uint8_t function, uint16_t reg, uint16_t value) {
    uint8_t request[8];
    uint16_t crc;

    request[0] = address;
    request[1] = function;
    request[2] = (reg >> 8) & 0xFF;
    request[3] = reg & 0xFF;
    request[4] = (value >> 8) & 0xFF;
    request[5] = value & 0xFF;

    crc = calculate_crc(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;

    uart_write_bytes(UART_NUM, (const char *)request, 8);
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

void relay_control(uint8_t relay, uint8_t state) {
    uint16_t reg = 0x0000 + relay;
    uint16_t value = state ? 0xFF00 : 0x0000;
    modbus_send_command(1, 0x05, reg, value);
    // print modbus command send value  to the console
    ESP_LOGI("MODBUS COMMAND", "Address: 1, Function: 0x05, Register: 0x%04X, Value: 0x%04X", reg, value);
}