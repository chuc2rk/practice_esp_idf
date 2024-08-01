#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define MODBUS_UART_NUM UART_NUM_1
#define MODBUS_TXD_PIN 6
#define MODBUS_RXD_PIN 7
//#define MODBUS_RTS_PIN 9
#define MODBUS_SLAVE_ADDRESS 0xFF  // Default address, changed to 0xFF

#define RELAY_1_PIN 2
#define RELAY_2_PIN 3
#define RELAY_3_PIN 9
#define RELAY_4_PIN 5

#define OPTOCOUPLER_1_PIN 10
#define OPTOCOUPLER_2_PIN 11
#define OPTOCOUPLER_3_PIN 12
#define OPTOCOUPLER_4_PIN 13

#define UART_BUF_SIZE 1024

static const char *TAG = "modbus_slave";

// Function prototypes
void modbus_init(void);
void modbus_task(void *pvParameters);
uint16_t modbus_crc16(uint8_t *buffer, uint16_t buffer_length);
void handle_modbus_request(uint8_t *request, int request_length);
void set_relay(int relay_num, bool state);
uint8_t read_relay_status(int relay_num);
void set_device_address(uint8_t new_address);
uint8_t get_device_address(void);
uint8_t read_optocoupler_status(void);
void set_baud_rate(uint8_t baud_rate_code);
void set_relay_flashing_mode(uint8_t relay_num, uint16_t mode, uint16_t delay_time);

// Global variables
static uint8_t g_device_address = MODBUS_SLAVE_ADDRESS;
static TimerHandle_t relay_timers[4] = {NULL};

typedef struct {
    uint8_t relay_num;
    bool flash_state;
} RelayTimerParams;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Modbus Slave application");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load device address and baud rate from NVS
    nvs_handle_t my_handle;
    ret = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (ret == ESP_OK) {
        uint8_t stored_address;
        uint32_t stored_baud_rate;
        if (nvs_get_u8(my_handle, "dev_address", &stored_address) == ESP_OK) {
            g_device_address = stored_address;
        }
        if (nvs_get_u32(my_handle, "baud_rate", &stored_baud_rate) == ESP_OK) {
            set_baud_rate(stored_baud_rate);
        }
        nvs_close(my_handle);
    }

    // Initialize GPIOs for relays
    gpio_reset_pin(RELAY_1_PIN);
    gpio_reset_pin(RELAY_2_PIN);
    gpio_reset_pin(RELAY_3_PIN);
    gpio_reset_pin(RELAY_4_PIN);
    gpio_set_direction(RELAY_1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_3_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_4_PIN, GPIO_MODE_OUTPUT);

    // Initialize GPIOs for optocouplers
    gpio_reset_pin(OPTOCOUPLER_1_PIN);
    gpio_reset_pin(OPTOCOUPLER_2_PIN);
    gpio_reset_pin(OPTOCOUPLER_3_PIN);
    gpio_reset_pin(OPTOCOUPLER_4_PIN);
    gpio_set_direction(OPTOCOUPLER_1_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(OPTOCOUPLER_2_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(OPTOCOUPLER_3_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(OPTOCOUPLER_4_PIN, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "GPIO pins initialized for relays and optocouplers");

    // Initialize Modbus
    modbus_init();

    // Create Modbus task with increased stack size
    xTaskCreate(modbus_task, "modbus_task", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "Modbus task created");
}

void modbus_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_driver_install(MODBUS_UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(MODBUS_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(MODBUS_UART_NUM, MODBUS_TXD_PIN, MODBUS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Set RS485 half-duplex mode
    ESP_ERROR_CHECK(uart_set_mode(MODBUS_UART_NUM, UART_MODE_RS485_HALF_DUPLEX));

    ESP_LOGI(TAG, "Modbus UART initialized");
}

void modbus_task(void *pvParameters)
{
    uint8_t* data = (uint8_t*) malloc(UART_BUF_SIZE);

    ESP_LOGI(TAG, "Modbus task started");

    while (1) {
        int len = uart_read_bytes(MODBUS_UART_NUM, data, UART_BUF_SIZE, 20 / portTICK_RATE_MS);
        if (len > 0) {
            ESP_LOGI(TAG, "Received %d bytes", len);
            ESP_LOG_BUFFER_HEX(TAG, data, len);
            handle_modbus_request(data, len);
        }
    }
    free(data);
}

uint16_t modbus_crc16(uint8_t *buffer, uint16_t buffer_length)
{
    uint16_t crc = 0xFFFF;
    
    for (uint16_t pos = 0; pos < buffer_length; pos++) {
        crc ^= (uint16_t)buffer[pos];
        
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
                crc >>= 1;
        }
    }
    
    return ((crc >> 8) | (crc << 8));  // Swap bytes before returning
}

void handle_modbus_request(uint8_t *request, int request_length)
{
    if (request_length < 8) {
        ESP_LOGW(TAG, "Received frame too short");
        return;
    }

    uint8_t slave_address = request[0];
    uint8_t function_code = request[1];
    uint16_t start_address = (request[2] << 8) | request[3];
    uint16_t received_crc = (request[request_length - 2] << 8) | request[request_length - 1];

    uint16_t calculated_crc = modbus_crc16(request, request_length - 2);

    ESP_LOGI(TAG, "Slave Address: 0x%02X, Function Code: 0x%02X, Start Address: %d", slave_address, function_code, start_address);
    ESP_LOGI(TAG, "Received CRC: 0x%04X, Calculated CRC: 0x%04X", received_crc, calculated_crc);

    if (received_crc != calculated_crc) {
        ESP_LOGW(TAG, "CRC error");
        return;
    }

    // Allow broadcast address (0x00) and the specific device address
    if (slave_address != 0x00 && slave_address != g_device_address) {
        ESP_LOGW(TAG, "Wrong slave address");
        return;
    }

    uint8_t response[256];
    int response_length = 0;

    response[0] = g_device_address;
    response[1] = function_code;

    switch (function_code) {
        case 0x01:  // Read Coils (Read relay status)
            {
                uint16_t quantity = (request[4] << 8) | request[5];
                if (start_address + quantity > 8) {
                    response[1] |= 0x80;
                    response[2] = 0x02;  // Illegal data address
                    response_length = 3;
                } else {
                    response[2] = 1;  // Byte count
                    response[3] = 0;
                    for (int i = 0; i < 8; i++) {
                        if (read_relay_status(i)) {
                            response[3] |= (1 << i);
                        }
                    }
                    response_length = 4;
                }
            }
            break;

        case 0x02:  // Read Discrete Inputs (Read optocoupler input status)
            {
                uint16_t quantity = (request[4] << 8) | request[5];
                if (start_address + quantity > 8) {
                    response[1] |= 0x80;
                    response[2] = 0x02;  // Illegal data address
                    response_length = 3;
                } else {
                    response[2] = 1;  // Byte count
                    response[3] = read_optocoupler_status();
                    response_length = 4;
                }
            }
            break;

        case 0x03:  // Read Holding Registers (Read device address)
            if (start_address == 0x0000 && ((request[4] << 8) | request[5]) == 0x0001) {
                response[2] = 2;  // Byte count
                response[3] = 0x00;
                response[4] = g_device_address;
                response_length = 5;
            } else {
                response[1] |= 0x80;
                response[2] = 0x02;  // Illegal data address
                response_length = 3;
            }
            break;

        case 0x05:  // Write Single Coil (Control single relay)
            {
                uint16_t coil_value = (request[4] << 8) | request[5];
                if (start_address <= 3) {
                    if (coil_value == 0xFF00) {
                        set_relay(start_address + 1, true);
                    } else if (coil_value == 0x0000) {
                        set_relay(start_address + 1, false);
                    } else {
                        response[1] |= 0x80;
                        response[2] = 0x03;  // Illegal data value
                        response_length = 3;
                        break;
                    }
                    memcpy(response + 2, request + 2, 4);  // Echo back the address and value
                    response_length = 6;
                } else {
                    response[1] |= 0x80;
                    response[2] = 0x02;  // Illegal data address
                    response_length = 3;
                }
            }
            break;

        case 0x0F:  // Write Multiple Coils (Control all relays)
            {
                uint16_t start_coil = (request[2] << 8) | request[3];
                uint16_t quantity = (request[4] << 8) | request[5];
                uint8_t byte_count = request[6];
                uint8_t coil_value = request[7];

                if (start_coil == 0 && quantity == 8 && byte_count == 1) {
                    for (int i = 0; i < 4; i++) {
                        set_relay(i + 1, coil_value & (1 << i));
                    }
                    memcpy(response + 2, request + 2, 4);  // Echo back start address and quantity
                    response_length = 6;
                } else {
                    response[1] |= 0x80;
                    response[2] = 0x02;  // Illegal data address
                    response_length = 3;
                }
            }
            break;

        case 0x10:  // Write Multiple Registers
            {
                uint16_t start_register = (request[2] << 8) | request[3];
                uint16_t quantity = (request[4] << 8) | request[5];
                uint8_t byte_count = request[6];

                if (start_register == 0x0000 && quantity == 0x0001 && byte_count == 0x02) {
                    // Set device address
                    uint8_t new_address = request[8];
                    set_device_address(new_address);
                    memcpy(response + 2, request + 2, 4);  // Echo back start address and quantity
                    response_length = 6;
                } else if (start_register == 0x03E9 && quantity == 0x0001 && byte_count == 0x02) {
                    // Set baud rate
                    uint8_t baud_rate_code = request[8];
                    set_baud_rate(baud_rate_code);
                    memcpy(response + 2, request + 2, 4);  // Echo back start address and quantity
                    response_length = 6;
                } else if ((start_register == 0x0003 || start_register == 0x0008 || 
                            start_register == 0x000D || start_register == 0x0012) && 
                           quantity == 0x0002 && byte_count == 0x04) {
                    // Set relay flashing mode
                    uint8_t relay_num = (start_register - 0x0003) / 5 + 1;
                    uint16_t mode = (request[7] << 8) | request[8];
                    uint16_t delay_time = (request[9] << 8) | request[10];
                    set_relay_flashing_mode(relay_num, mode, delay_time);                    
                    memcpy(response + 2, request + 2, 4);  // Echo back start address and quantity
                    response_length = 6;
                } else {
                    response[1] |= 0x80;
                    response[2] = 0x02;  // Illegal data address
                    response_length = 3;
                }
            }
            break;

        default:
            ESP_LOGW(TAG, "Unsupported function code");
            response[1] |= 0x80;  // Error response
            response[2] = 0x01;   // Illegal function
            response_length = 3;
            break;
    }

  
    uint16_t response_crc = modbus_crc16(response, response_length);
    response[response_length] = response_crc >> 8;
    response[response_length + 1] = response_crc & 0xFF;
    response_length += 2;
    ESP_LOGI(TAG, "Sending response");
    ESP_LOG_BUFFER_HEX(TAG, response, response_length);
    uart_write_bytes(MODBUS_UART_NUM, (const char*)response, response_length);
    
}

void set_relay(int relay_num, bool state)
{
    if (relay_num < 1 || relay_num > 4) {
        ESP_LOGW(TAG, "Invalid relay number: %d", relay_num);
        return;
    }

    gpio_num_t relay_pin;
    switch (relay_num) {
        case 1: relay_pin = RELAY_1_PIN; break;
        case 2: relay_pin = RELAY_2_PIN; break;
        case 3: relay_pin = RELAY_3_PIN; break;
        case 4: relay_pin = RELAY_4_PIN; break;
        default: return;  // This should never happen due to the check above
    }
    gpio_set_level(relay_pin, state ? 1 : 0);
    ESP_LOGI(TAG, "Relay %d set to %s", relay_num, state ? "ON" : "OFF");
}

uint8_t read_relay_status(int relay_num)
{
    if (relay_num < 0 || relay_num > 3) {
        ESP_LOGW(TAG, "Invalid relay number: %d", relay_num);
        return 0;
    }

    gpio_num_t relay_pin;
    switch (relay_num) {
        case 0: relay_pin = RELAY_1_PIN; break;
        case 1: relay_pin = RELAY_2_PIN; break;
        case 2: relay_pin = RELAY_3_PIN; break;
        case 3: relay_pin = RELAY_4_PIN; break;
        default: return 0;  // This should never happen due to the check above
    }
    return gpio_get_level(relay_pin);
}

void set_device_address(uint8_t new_address)
{
    if ((new_address >= 1 && new_address <= 247) || new_address == 0xFF) {
        g_device_address = new_address;
        
        // Save the new address to NVS
        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
        if (err == ESP_OK) {
            err = nvs_set_u8(my_handle, "dev_address", new_address);
            if (err == ESP_OK) {
                err = nvs_commit(my_handle);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Device address saved to NVS");
                }
            }
            nvs_close(my_handle);
        }
        
        ESP_LOGI(TAG, "Device address set to: %d", new_address);
    } else {
        ESP_LOGW(TAG, "Invalid device address: %d", new_address);
    }
}

uint8_t get_device_address(void)
{
    return g_device_address;
}

uint8_t read_optocoupler_status(void)
{
    uint8_t status = 0;
    status |= gpio_get_level(OPTOCOUPLER_1_PIN) << 0;
    status |= gpio_get_level(OPTOCOUPLER_2_PIN) << 1;
    status |= gpio_get_level(OPTOCOUPLER_3_PIN) << 2;
    status |= gpio_get_level(OPTOCOUPLER_4_PIN) << 3;
    return status;
}

void set_baud_rate(uint8_t baud_rate_code)
{
    uint32_t new_baud_rate;
    switch (baud_rate_code) {
        case 0x02:
            new_baud_rate = 4800;
            break;
        case 0x03:
            new_baud_rate = 9600;
            break;
        case 0x04:
            new_baud_rate = 19200;
            break;
        default:
            ESP_LOGW(TAG, "Unsupported baud rate code: %d", baud_rate_code);
            return;
    }
    
    uart_set_baudrate(MODBUS_UART_NUM, new_baud_rate);
    
    // Save the new baud rate to NVS
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_u32(my_handle, "baud_rate", new_baud_rate);
        if (err == ESP_OK) {
            err = nvs_commit(my_handle);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Baud rate saved to NVS");
            }
        }
        nvs_close(my_handle);
    }
    
    ESP_LOGI(TAG, "Baud rate set to: %d", new_baud_rate);
}

void relay_timer_callback(TimerHandle_t xTimer)
{
    RelayTimerParams *params = (RelayTimerParams*)pvTimerGetTimerID(xTimer);
    params->flash_state = !params->flash_state;
    set_relay(params->relay_num, params->flash_state);
    ESP_LOGI(TAG, "Relay %d set to %s", params->relay_num, params->flash_state ? "ON" : "OFF");
}

void set_relay_flashing_mode(uint8_t relay_num, uint16_t mode, uint16_t delay_time)
{
    if (relay_num < 1 || relay_num > 4) {
        ESP_LOGW(TAG, "Invalid relay number: %d", relay_num);
        return;
    }
    
    // Delete existing timer for this relay if it exists
    if (relay_timers[relay_num - 1] != NULL) {
        xTimerDelete(relay_timers[relay_num - 1], 0);
        relay_timers[relay_num - 1] = NULL;
    }
    
    // If delay_time is 0, just set the relay state and return
    if (delay_time == 0) {
        set_relay(relay_num, mode == 0x0001 || mode == 0x0003);
        ESP_LOGI(TAG, "Relay %d set to %s", relay_num, (mode == 0x0001 || mode == 0x0003) ? "ON" : "OFF");
        return;
    }

    RelayTimerParams *params = malloc(sizeof(RelayTimerParams));
    if (params == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relay timer parameters");
        return;
    }
    params->relay_num = relay_num;
    params->flash_state = (mode == 0x0001 || mode == 0x0003);
    
    float delay_seconds = delay_time * 0.1;
    
    relay_timers[relay_num - 1] = xTimerCreate("relay_timer", pdMS_TO_TICKS(delay_seconds * 1000),
                                               pdTRUE, params, relay_timer_callback);
    
    if (relay_timers[relay_num - 1] == NULL) {
        ESP_LOGE(TAG, "Failed to create timer for relay %d", relay_num);
        free(params);
        return;
    }
    
    // Set initial state based on mode
    set_relay(relay_num, params->flash_state);
    
    if (xTimerStart(relay_timers[relay_num - 1], 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer for relay %d", relay_num);
        xTimerDelete(relay_timers[relay_num - 1], 0);
        relay_timers[relay_num - 1] = NULL;
        free(params);
        return;
    }
    
    ESP_LOGI(TAG, "Relay %d set to flashing mode %d with delay %.1f seconds", relay_num, mode, delay_seconds);
}