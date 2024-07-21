#ifndef MODBUS_H
#define MODBUS_H

#include <stdint.h>
#include "esp_err.h"

void uart_init(void);
void modbus_send_command(uint8_t address, uint8_t function, uint16_t reg, uint16_t value);
uint16_t calculate_crc(uint8_t *data, uint16_t length);
void relay_control(uint8_t relay, uint8_t state);

#endif // MODBUS_H
