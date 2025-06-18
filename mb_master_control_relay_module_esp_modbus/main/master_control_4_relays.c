#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "modbus_params.h"  // for modbus parameters structures
#include "mbcontroller.h"
#include "esp_modbus_master.h"
#include "sdkconfig.h"

#define MB_PORT_NUM     (CONFIG_MB_UART_PORT_NUM)
#define MB_DEV_SPEED    (9600)

#define MB_SLAVE_ADDR   (0x01)

#define MB_READ_HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) + 1))
#define MB_WRITE_COIL_OFFSET(field) ((uint16_t)(offsetof(coil_reg_params_t, field) + 1))

static const char *TAG = "MODBUS_MASTER";

// Enumeration of modbus device addresses accessed by master device
enum {
    MB_DEVICE_ADDR1 = 1
};

// Enumeration of all supported CIDs for device
enum {
    CID_HOLD_DEV_ADDR = 0,
    CID_HOLD_SOFTWARE_VER,
    CID_HOLD_HARDWARE_VER,
    CID_COIL_RELAY0,
    CID_COIL_RELAY1,
    CID_COIL_RELAY2,
    CID_COIL_RELAY3,
    CID_DISC_INPUT1,
    CID_DISC_INPUT2,
    CID_DISC_INPUT3,
    CID_DISC_INPUT4
};

// Example Data (Object) Dictionary for Modbus parameters:
const mb_parameter_descriptor_t device_parameters[] = {
    // CID, Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode
    { CID_HOLD_DEV_ADDR, "DEV_ADDR", "ADDR", MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0x4000, 1,
      MB_READ_HOLD_OFFSET(dev_addr), PARAM_TYPE_U16, 2, OPTS(0, 255, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    
    { CID_HOLD_SOFTWARE_VER, "SOFTWARE_VER", "VER", MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0x0002, 4,
      MB_READ_HOLD_OFFSET(software_ver), PARAM_TYPE_ASCII, 8, OPTS(0, 0, 0), PAR_PERMS_READ },
    
    { CID_HOLD_HARDWARE_VER, "HARDWARE_VER", "VER", MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0x0020, 1,
      MB_READ_HOLD_OFFSET(hardware_ver), PARAM_TYPE_U16, 2, OPTS(0, 0, 0), PAR_PERMS_READ },
    
    { CID_COIL_RELAY0, "RELAY0", "ON/OFF", MB_DEVICE_ADDR1, MB_PARAM_COIL, 0x0000, 1,
      MB_WRITE_COIL_OFFSET(relay0), PARAM_TYPE_U8, 1, OPTS(0, 1, 0), PAR_PERMS_READ_WRITE_TRIGGER },
    
    { CID_COIL_RELAY1, "RELAY1", "ON/OFF", MB_DEVICE_ADDR1, MB_PARAM_COIL, 0x0001, 1,
      MB_WRITE_COIL_OFFSET(relay1), PARAM_TYPE_U8, 1, OPTS(0, 1, 0), PAR_PERMS_READ_WRITE_TRIGGER },
    
    { CID_COIL_RELAY2, "RELAY2", "ON/OFF", MB_DEVICE_ADDR1, MB_PARAM_COIL, 0x0002, 1,
      MB_WRITE_COIL_OFFSET(relay2), PARAM_TYPE_U8, 1, OPTS(0, 1, 0), PAR_PERMS_READ_WRITE_TRIGGER },
    
    { CID_COIL_RELAY3, "RELAY3", "ON/OFF", MB_DEVICE_ADDR1, MB_PARAM_COIL, 0x0003, 1,
      MB_WRITE_COIL_OFFSET(relay3), PARAM_TYPE_U8, 1, OPTS(0, 1, 0), PAR_PERMS_READ_WRITE_TRIGGER },
    
    { CID_DISC_INPUT1, "INPUT1", "ON/OFF", MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 0x0000, 1,
      0, PARAM_TYPE_U8, 1, OPTS(0, 1, 0), PAR_PERMS_READ },
    
    { CID_DISC_INPUT2, "INPUT2", "ON/OFF", MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 0x0001, 1,
      0, PARAM_TYPE_U8, 1, OPTS(0, 1, 0), PAR_PERMS_READ },
    
    { CID_DISC_INPUT3, "INPUT3", "ON/OFF", MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 0x0002, 1,
      0, PARAM_TYPE_U8, 1, OPTS(0, 1, 0), PAR_PERMS_READ },
    
    { CID_DISC_INPUT4, "INPUT4", "ON/OFF", MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 0x0003, 1,
      0, PARAM_TYPE_U8, 1, OPTS(0, 1, 0), PAR_PERMS_READ }
};

// Calculate number of parameters in the table
const uint16_t num_device_parameters = (sizeof(device_parameters) / sizeof(device_parameters[0]));

// Function to set device address
esp_err_t set_device_address(uint8_t new_address)
{
    uint8_t type = 0;
    esp_err_t err = mbc_master_set_parameter(CID_HOLD_DEV_ADDR, "DEV_ADDR", (uint8_t*)&new_address, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Device address set to: %d", new_address);
    } else {
        ESP_LOGE(TAG, "Failed to set device address");
    }
    return err;
}

// Function to read device address
esp_err_t read_device_address()
{
    uint8_t type = 0;
    uint16_t address_data;
    esp_err_t err = mbc_master_get_parameter(CID_HOLD_DEV_ADDR, "DEV_ADDR", (uint8_t*)&address_data, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Device address: %d", address_data);
    } else {
        ESP_LOGE(TAG, "Failed to read device address");
    }
    return err;
}

// Function to read software version
esp_err_t read_software_version()
{
    uint8_t type = 0;
    char version_data[8];
    esp_err_t err = mbc_master_get_parameter(CID_HOLD_SOFTWARE_VER, "SOFTWARE_VER", (uint8_t*)version_data, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Software version: %s", version_data);
    } else {
        ESP_LOGE(TAG, "Failed to read software version");
    }
    return err;
}

// Function to read hardware version
esp_err_t read_hardware_version()
{
    uint8_t type = 0;
    uint16_t version_data;
    esp_err_t err = mbc_master_get_parameter(CID_HOLD_HARDWARE_VER, "HARDWARE_VER", (uint8_t*)&version_data, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Hardware version: V%d.%02d", version_data / 100, version_data % 100);
    } else {
        ESP_LOGE(TAG, "Failed to read hardware version");
    }
    return err;
}

// Function to control relay (write single coil)
esp_err_t control_relay(uint8_t relay_num, bool state)
{
    uint8_t type = 0;
    uint16_t coil_data = state ? 0xFF00 : 0x0000;
    const char* relay_names[] = {"RELAY0", "RELAY1", "RELAY2", "RELAY3"};
    
    mb_param_request_t request = {
        .slave_addr = MB_SLAVE_ADDR,
        .command = MB_FUNC_WRITE_SINGLE_COIL,
        .reg_start = relay_num,
        .reg_size = 1
    };
    
    esp_err_t err = mbc_master_send_request(&request, &coil_data);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Relay %d set to %s", relay_num, state ? "ON" : "OFF");
    } else {
        ESP_LOGE(TAG, "Failed to control relay %d", relay_num);
    }
    return err;
}

// Function to read relay status
esp_err_t read_relay_status(uint8_t relay_num)
{
    uint8_t type = 0;
    uint8_t coil_data;
    const char* relay_names[] = {"RELAY0", "RELAY1", "RELAY2", "RELAY3"};
    esp_err_t err = mbc_master_get_parameter(CID_COIL_RELAY0 + relay_num, relay_names[relay_num], &coil_data, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Relay %d status: %s", relay_num, coil_data ? "ON" : "OFF");
    } else {
        ESP_LOGE(TAG, "Failed to read relay %d status", relay_num);
    }
    return err;
}

// Function to read all input statuses
esp_err_t read_all_inputs()
{
    uint8_t type = 0;
    uint8_t input_data;
    const char* input_names[] = {"INPUT1", "INPUT2", "INPUT3", "INPUT4"};
    for (int i = 0; i < 4; i++) {
        esp_err_t err = mbc_master_get_parameter(CID_DISC_INPUT1 + i, input_names[i], &input_data, &type);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Input %d status: %s", i+1, input_data ? "PRESSED" : "RELEASED");
        } else {
            ESP_LOGE(TAG, "Failed to read input %d status", i+1);
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t err;

    // Initialize and start Modbus controller
    mb_communication_info_t comm = {
        .port = MB_PORT_NUM,
        .mode = MB_MODE_RTU,
        .baudrate = MB_DEV_SPEED,
        .parity = MB_PARITY_NONE
    };

    void* master_handler = NULL;

    err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb controller initialization fail");

    err = mbc_master_setup((void*)&comm);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb controller setup fail");

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD, CONFIG_MB_UART_RXD,
                       CONFIG_MB_UART_RTS, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb serial set pin failure");

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb controller start fail");

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb serial set mode failure");

    // Set Modbus parameters
    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb controller set descriptor fail");

    // Example usage of functions
    vTaskDelay(100 / portTICK_PERIOD_MS);
    set_device_address(2);
    read_device_address();
    read_software_version();
    read_hardware_version();
    control_relay(0, true);
    read_relay_status(0);
    read_all_inputs();

    // Clean up (in practice, you might want to keep the Modbus master running)
    mbc_master_destroy();
}