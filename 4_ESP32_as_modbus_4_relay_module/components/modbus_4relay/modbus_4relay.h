#ifndef MODBUS_4RELAY_H
#define MODBUS_4RELAY_H

#include <stdint.h>

void uart_init(void);
void modbus_task(void *arg);
uint16_t calculate_crc(uint8_t *data, uint16_t length);
void handle_modbus_command(uint8_t *data, uint16_t length);

#endif // MODBUS_MASTER_H
