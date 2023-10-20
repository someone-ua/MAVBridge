#include "modem.h"
#include <esp_wifi_types.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "freertos/event_groups.h"
#include <driver/gpio.h>
#include "esp_event.h"
#include <net/if.h>
#include "esp_log.h"

#define TAG "Modem"
#define HIGH 1
#define LOW 0

static const int CONNECT_BIT = BIT0;
static const int GOT_DATA_BIT = BIT2;

static EventGroupHandle_t event_group = NULL;
esp_modem_dce_t *dce;

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %li", event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}


static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %li", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

void init_modem() {
    /* Init and register system/core components */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    gpio_set_direction(CONFIG_MODEM_POWER_PIN,GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_MODEM_RESET_PIN,GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_MODEM_PWKEY_PIN,GPIO_MODE_OUTPUT);

    gpio_set_level(CONFIG_MODEM_RESET_PIN,HIGH);
    vTaskDelay(1000 /portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_MODEM_PWKEY_PIN,HIGH);
    vTaskDelay(1000 /portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_MODEM_POWER_PIN,HIGH);
    vTaskDelay(1000 /portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_MODEM_PWKEY_PIN,LOW);
    vTaskDelay(1000 /portTICK_PERIOD_MS);

    /* Configure the PPP netif */
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_MODEM_PPP_APN);
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    ESP_LOGI(TAG,"%s",netif_ppp_config.base->if_key);
    ESP_LOGI(TAG,"%s",netif_ppp_config.base->if_desc);
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    event_group = xEventGroupCreate();

    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    dte_config.uart_config.tx_io_num = CONFIG_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_MODEM_UART_RX_PIN;

    ESP_LOGI(TAG, "Initializing esp_modem for the SIM800 module...");
    dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM800, &dte_config, &dce_config, esp_netif);

    xEventGroupClearBits(event_group, CONNECT_BIT | GOT_DATA_BIT);

    vTaskDelay(5000 /portTICK_PERIOD_MS);

    esp_err_t err = esp_modem_set_mode(dce, ESP_MODEM_MODE_CMUX);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_CMUX) failed with %d", err);
        return;
    }
    /* Wait for IP address */
    ESP_LOGI(TAG, "Waiting for IP address");
    xEventGroupWaitBits(event_group, CONNECT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}
