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


static const char *TAG ="ethernet";
#define ETH_PHY_ADDR             1
#define ETH_PHY_RST_GPIO        -1          // not connected
#define ETH_MDC_GPIO            23
#define ETH_MDIO_GPIO           18

/** Event handler for Ethernet events */
static void eth_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
        //get MAC address
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;

    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;

    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;

    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

void app_main (void)
{
    /* 
     * Purpose: This function initializes the TCP/IP network interface for the ESP32. 
     * In simpler terms, it sets up the networking stack so that the ESP32 can communicate over network protocols like Wi-Fi, Ethernet, etc.
     * Why it's needed: Before any network communication can occur (like connecting to a Wi-Fi network or sending/receiving data over the network), 
     * the underlying networking subsystem needs to be initialized. 
     * This function ensures that all necessary networking components are ready to use.
     * Why it's needed: Before any network communication can occur (like connecting to a Wi-Fi network or sending/receiving data over the network), 
     * the underlying networking subsystem needs to be initialized. 
     * This function ensures that all necessary networking components are ready to use.
     * */
    ESP_ERROR_CHECK(esp_netif_init());

    /*
     * Purpose: This function creates a default event loop that runs in the background. 
     * An event loop is responsible for handling and dispatching events, 
     * such as network status changes, timers, or custom application events.
     * Why it's needed: In ESP-IDF, events are a common way to handle asynchronous operations. 
     * For example, when the ESP32 connects to a Wi-Fi network, 
     * an event is triggered to notify the application. 
     * The default event loop manages these events, ensuring they are processed appropriately without blocking other tasks.
     */
    ESP_ERROR_CHECK(esp_event_loop_create_default());   


    /*
        Create MAC and PHY Instance
    */
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


    /*
      Install Driver
    */
    // apply default driver configuration
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy); 
    // after driver installed, we will get the handle of the driver
    esp_eth_handle_t eth_handle = NULL;
    //install driver
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));


    /*
        To connect Ethernet driver to TCP/IP stack, these three steps need to follow:
        1. Create network interface for Ethernet driver
        2. Attach the network interface to Ethernet driver
        3.Register IP event handlers
    */
    // Create new default instance of esp-netif for Ethernet
    // apply default network interface configuration for Ethernet
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    // create network interface for Ethernet driver
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    // start Ethernet driver state machine
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}