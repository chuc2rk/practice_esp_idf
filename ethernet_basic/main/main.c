#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"


static const char *TAG ="ethernet"
#define ETH_PHY_ADDR             1
#define ETH_PHY_RST_GPIO        -1          // not connected
#define ETH_MDC_GPIO            23
#define ETH_MDIO_GPIO           18

void app_main (void)
{
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create new default instance of esp-netif for Ethernet
    // apply default network interface configuration for Ethernet
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    // create network interface for Ethernet driver
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    //Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    /*
    multiple PHY device can share the same SMI bus, so each PHY needs a unique address. 
    Usually this address is configured during hardware design by pulling up/down some PHY strapping pins.
    You can set the value from 0 to 15 based on your Ethernet board. 
    Especially, if the SMI bus is shared by only one PHY device, 
    setting this value to -1 can enable the driver to detect the PHY address automatically.
    */
    phy_config.phy_addr = ETH_PHY_ADDR;
    /*
    if your board also connect the PHY reset pin to one of the GPIO, 
    then set it here. Otherwise, set this field to -1, means no hardware reset
    */
    phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;

    //SMI MDC GPIO number, set to -1 could bypass the SMI GPIO configuration
    mac_config.smi_mdc_gpio_num = ETH_MDC_GPIO;
    //SMI MDIO GPIO number, set to -1 could bypass the SMI GPIO configuration
    mac_config.smi_mdio_gpio_num = ETH_MDIO_GPIO;
    
    // create MAC instance
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
    // create PHY instance
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    // apply default driver configuration
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy); 
    // after driver installed, we will get the handle of the driver
    esp_eth_handle_t eth_handle = NULL;
    //install driver
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    


}