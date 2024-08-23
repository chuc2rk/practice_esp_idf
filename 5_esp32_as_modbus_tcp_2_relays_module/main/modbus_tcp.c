#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "connect.h"
#include "lan8720.h"

#define MODBUS_TCP_PORT 502
#define MODBUS_SLAVE_ADDRESS 0x01  // Default address, changed to 0xFF

#define RELAY_1_PIN 4
#define RELAY_2_PIN 5

#define OPTOCOUPLER_1_PIN 12
#define OPTOCOUPLER_2_PIN 13

#define TCP_BUF_SIZE 1024

static const char *TAG = "modbus_slave";

// Function prototypes
void modbus_tcp_task(void *pvParameters);
void handle_modbus_request(uint8_t *request, int request_length, int sock);
void set_relay(int relay_num, bool state);
uint8_t read_relay_status(int relay_num);
void set_device_address(uint8_t new_address);
uint8_t get_device_address(void);
uint8_t read_optocoupler_status(void);
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
    ESP_LOGI(TAG, "Starting Modbus TCP Slave application");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load device address from NVS
    nvs_handle_t my_handle;
    ret = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (ret == ESP_OK) {
        uint8_t stored_address;
        if (nvs_get_u8(my_handle, "dev_address", &stored_address) == ESP_OK) {
            g_device_address = stored_address;
        }
        nvs_close(my_handle);
    }      
    
    
    // Initialize GPIOs for relays
    gpio_pad_select_gpio(RELAY_1_PIN);
    gpio_set_direction(RELAY_1_PIN, GPIO_MODE_OUTPUT);
    
    gpio_pad_select_gpio(RELAY_2_PIN);
    gpio_set_direction(RELAY_2_PIN, GPIO_MODE_OUTPUT);

    // Initialize GPIOs for optocouplers
    gpio_pad_select_gpio(OPTOCOUPLER_1_PIN);
    gpio_set_direction(OPTOCOUPLER_1_PIN, GPIO_MODE_INPUT);

    gpio_pad_select_gpio(OPTOCOUPLER_2_PIN);
    gpio_set_direction(OPTOCOUPLER_2_PIN, GPIO_MODE_INPUT);
    ESP_LOGI(TAG, "GPIO pins initialized for relays and optocouplers");

    // Initialize WiFi or Ethernet here
    wifi_init();
    ESP_ERROR_CHECK(wifi_connect_sta("ssid", "pass", 10000));
    //ESP_ERROR_CHECK(ethernet(10000));

    // Create Modbus TCP task
    xTaskCreate(modbus_tcp_task, "modbus_tcp_task", 4096, NULL, 5, NULL);
}

void modbus_tcp_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(MODBUS_TCP_PORT);
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        uint8_t rx_buffer[TCP_BUF_SIZE];
        int len;

        do {
            len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            } else if (len == 0) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            } else {
                ESP_LOGI(TAG, "Received %d bytes", len);
                ESP_LOG_BUFFER_HEX(TAG, rx_buffer, len);
                handle_modbus_request(rx_buffer, len, sock);
            }
        } while (len > 0);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void handle_modbus_request(uint8_t *request, int request_length, int sock)
{
    if (request_length < 8) {
        ESP_LOGW(TAG, "Received frame too short");
        return;
    }

    uint16_t transaction_id = (request[0] << 8) | request[1];
    uint16_t protocol_id = (request[2] << 8) | request[3];
    uint16_t length = (request[4] << 8) | request[5];
    uint8_t unit_id = request[6];
    uint8_t function_code = request[7];
    uint16_t start_address = (request[8] << 8) | request[9];

    ESP_LOGI(TAG, "Transaction ID: %d, Protocol ID: %d, Length: %d, Unit ID: %d, Function Code: 0x%02X, Start Address: %d",
             transaction_id, protocol_id, length, unit_id, function_code, start_address);

    // Allow broadcast address (0x00) and the specific device address
    if (unit_id != 0x00 && unit_id != g_device_address) {
        ESP_LOGW(TAG, "Wrong unit ID");
        return;
    }

    uint8_t response[256];
    int response_length = 0;

    // Set Modbus TCP header
    memcpy(response, request, 7);

    switch (function_code) {
        case 0x01:  // Read Coils (Read relay status)
            {
                uint16_t quantity = (request[10] << 8) | request[11];
                if (start_address + quantity > 8) {
                    response[7] = function_code | 0x80;
                    response[8] = 0x02;  // Illegal data address
                    response_length = 9;
                } else {
                    response[7] = function_code;
                    response[8] = 1;  // Byte count
                    response[9] = 0;
                    for (int i = 0; i < 2; i++) {
                        if (read_relay_status(i)) {
                            response[9] |= (1 << i);
                        }
                    }
                    response_length = 10;
                }
            }
            break;

        case 0x02:  // Read Discrete Inputs (Read optocoupler input status)
            {
                uint16_t quantity = (request[10] << 8) | request[11];
                if (start_address + quantity > 8) {
                    response[7] = function_code | 0x80;
                    response[8] = 0x02;  // Illegal data address
                    response_length = 9;
                } else {
                    response[7] = function_code;
                    response[8] = 1;  // Byte count
                    response[9] = read_optocoupler_status();
                    response_length = 10;
                }
            }
            break;

        case 0x03:  // Read Holding Registers (Read device address)
            if (start_address == 0x0000 && ((request[10] << 8) | request[11]) == 0x0001) {
                response[7] = function_code;
                response[8] = 2;  // Byte count
                response[9] = 0x00;
                response[10] = g_device_address;
                response_length = 11;
            } else {
                response[7] = function_code | 0x80;
                response[8] = 0x02;  // Illegal data address
                response_length = 9;
            }
            break;

        case 0x05:  // Write Single Coil (Control single relay)
            {
                uint16_t coil_value = (request[10] << 8) | request[11];
                if (start_address <= 3) {
                    if (coil_value == 0xFF00) {
                        set_relay(start_address + 1, true);
                    } else if (coil_value == 0x0000) {
                        set_relay(start_address + 1, false);
                    } else {
                        response[7] = function_code | 0x80;
                        response[8] = 0x03;  // Illegal data value
                        response_length = 9;
                        break;
                    }
                    memcpy(response + 7, request + 7, 5);  // Echo the request
                    response_length = 12;
                } else {
                    response[7] = function_code | 0x80;
                    response[8] = 0x02;  // Illegal data address
                    response_length = 9;
                }
            }
            break;

        case 0x0F:  // Write Multiple Coils (Control all relays)
            {
                uint16_t start_coil = (request[8] << 8) | request[9];
                uint16_t quantity = (request[10] << 8) | request[11];
                uint8_t byte_count = request[12];
                uint8_t coil_value = request[13];

                if (start_coil == 0 && quantity == 8 && byte_count == 1) {
                    for (int i = 0; i < 2; i++) {
                        set_relay(i + 1, coil_value & (1 << i));
                    }
                    memcpy(response + 8, request + 8, 4);  // Echo back start address and quantity
                    response_length = 12;
                } else {
                    response[7] = function_code | 0x80;
                    response[8] = 0x02;  // Illegal data address
                    response_length = 9;
                }
            }
            break;

        case 0x10:  // Write Multiple Registers
            {
                uint16_t start_register = (request[8] << 8) | request[9];
                uint16_t quantity = (request[10] << 8) | request[11];
                uint8_t byte_count = request[12];

                if (start_register == 0x0000 && quantity == 0x0001 && byte_count == 0x02) {
                    // Set device address
                    uint8_t new_address = request[14];
                    set_device_address(new_address);
                    memcpy(response + 7, request + 7, 5);  // Echo the request
                    response_length = 12;
                } else if ((start_register == 0x0003 || start_register == 0x0008 || 
                            start_register == 0x000D || start_register == 0x0012) && 
                           quantity == 0x0002 && byte_count == 0x04) {
                    // Set relay flashing mode
                    uint8_t relay_num = (start_register - 0x0003) / 5 + 1;
                    uint16_t mode = (request[13] << 8) | request[14];
                    uint16_t delay_time = (request[15] << 8) | request[16];
                    set_relay_flashing_mode(relay_num, mode, delay_time);                    
                    memcpy(response + 8, request + 8, 4);  // Echo back start address and quantity
                    response_length = 12;
                } else {
                    response[7] = function_code | 0x80;
                    response[8] = 0x02;  // Illegal data address
                    response_length = 9;
                }
            }
            break;

        default:
            ESP_LOGW(TAG, "Unsupported function code");
            response[7] = function_code | 0x80;  // Error response
            response[8] = 0x01;   // Illegal function
            response_length = 9;
            break;
    }

    // Set Modbus TCP header length
    response[4] = ((response_length - 6) >> 8) & 0xFF;
    response[5] = (response_length - 6) & 0xFF;

    ESP_LOGI(TAG, "Sending response");
    ESP_LOG_BUFFER_HEX(TAG, response, response_length);
    send(sock, response, response_length, 0);
}

// The rest of the functions (set_relay, read_relay_status, set_device_address, 
// get_device_address, read_optocoupler_status, relay_timer_callback, 
// set_relay_flashing_mode) remain the same as in your original code.

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
        
        default: return;  // This should never happen due to the check above
    }
    gpio_set_level(relay_pin, state ? 1 : 0);
    ESP_LOGI(TAG, "Relay %d set to %s", relay_num, state ? "ON" : "OFF");
}

uint8_t read_relay_status(int relay_num)
{
    if (relay_num < 0 || relay_num > 1) {
        ESP_LOGW(TAG, "Invalid relay number: %d", relay_num);
        return 0;
    }

    gpio_num_t relay_pin;
    switch (relay_num) {
        case 0: relay_pin = RELAY_1_PIN; break;
        case 1: relay_pin = RELAY_2_PIN; break;
        
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
   
    return status;
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
    if (relay_num < 1 || relay_num > 2) {
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