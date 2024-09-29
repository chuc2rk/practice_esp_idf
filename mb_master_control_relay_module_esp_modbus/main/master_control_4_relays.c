
#include "string.h"
#include "esp_log.h"
#include "mbcontroller.h"

#define UART_NUM UART_NUM_1
#define TXD_PIN GPIO_NUM_6
#define RXD_PIN GPIO_NUM_7
#define RTS_PIN GPIO_NUM_8
#define BAUD_RATE 9600

#pragma pack(push, 1)
typedef struct
{
  uint8_t coil_relay1;
  uint8_t coil_relay2;
  uint8_t coil_relay3;
  uint8_t coil_relay4;
} coil_reg_params_t;
#pragma pack(pop)

coil_reg_params_t coil_reg_params = {0};

// The macro to get offset for parameter in the appropriate structure
#define COIL_OFFSET(field) ((uint16_t)(offsetof(coil_reg_params_t, field) + 1))

#define STR(fieldname) ((const char*)( fieldname ))
// Options can be used as bit masks or parameter limits
#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

enum {
  CID_COIL_RELAY1 = 0,
  CID_COIL_RELAY2,
  CID_COIL_RELAY3,
  CID_COIL_RELAY4;
  CID_COUNT;
};

// Enumeration of modbus device addresses accessed by master device
enum {
    MB_DEVICE_ADDR1 = 1 // Only one slave device used for the test (add other slave addresses here)
};


// The CID field in the table must be unique.
// Modbus Slave Addr field defines slave address of the device with correspond parameter.
// Modbus Reg Type - Type of Modbus register area (Holding register, Input Register and such).
// Reg Start field defines the start Modbus register number and Reg Size defines the number of registers for the characteristic accordingly.
// The Instance Offset defines offset in the appropriate parameter structure that will be used as instance to save parameter value.
// Data Type, Data Size specify type of the characteristic and its data size.
// Parameter Options field specifies the options that can be used to process parameter value (limits or masks).
// Access Mode - can be used to implement custom options for processing of characteristic (Read/Write restrictions, factory mode values and etc).
const mb_parameter_descriptor_t device_parameters[] = {
    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}    
    { CID_COIL_RELAY1, STR("Relay1"), STR("on/off"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 8,
            COIL_OFFSET(coils_port0), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0 ), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_COIL_RELAY2, STR("Relay2"), STR("on/off"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 8, 8,
            COIL_OFFSET(coils_port1), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0 ), PAR_PERMS_READ_WRITE_TRIGGER }
};

// Modbus master initialization
static esp_err_t master_init(void)
{
    // Initialize and start Modbus controller
    mb_communication_info_t comm = {
            .port = UART_NUM,
            .mode = MB_MODE_RTU,
            .baudrate = BAUD_RATE,
            .parity = MB_PARITY_NONE
    };
    void* master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG,
                                "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller initialization fail, returns(0x%x).",
                            (uint32_t)err);
    err = mbc_master_setup((void*)&comm);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller setup fail, returns(0x%x).",
                            (uint32_t)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, TXD_PIN, RXD_PIN,
                              UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
            "mb serial set pin failure, uart_set_pin() returned (0x%x).", (uint32_t)err);

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller start fail, returns(0x%x).",
                            (uint32_t)err);

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
            "mb serial set mode failure, uart_set_mode() returned (0x%x).", (uint32_t)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                                "mb controller set descriptor fail, returns(0x%x).",
                                (uint32_t)err);
    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}

void app_main(void)
{
    // Initialization of device peripheral and objects
    ESP_ERROR_CHECK(master_init());
    vTaskDelay(10);

    master_operation_func(NULL);
}
