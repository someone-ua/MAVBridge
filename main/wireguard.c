#include "wireguard.h"
#include <esp_wireguard.h>
#include "esp_log.h"

#define TAG "Wireguard"

void init_wireguard() {
    esp_err_t err = ESP_FAIL;

    wireguard_config_t wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();

    ESP_LOGI(TAG, "Initializing WireGuard.");
    wg_config.private_key = CONFIG_WG_PRIVATE_KEY;
    wg_config.listen_port = CONFIG_WG_LOCAL_PORT;
    wg_config.public_key = CONFIG_WG_PEER_PUBLIC_KEY;
    if (strcmp(CONFIG_WG_PRESHARED_KEY, "") != 0) {
        wg_config.preshared_key = CONFIG_WG_PRESHARED_KEY;
    } else {
        wg_config.preshared_key = NULL;
    }
    wg_config.allowed_ip = CONFIG_WG_LOCAL_IP_ADDRESS;
    wg_config.allowed_ip_mask = CONFIG_WG_LOCAL_IP_NETMASK;
    wg_config.endpoint = CONFIG_WG_PEER_ADDRESS;
    wg_config.port = CONFIG_WG_PEER_PORT;
    wg_config.persistent_keepalive = CONFIG_WG_PERSISTENT_KEEP_ALIVE;

    wireguard_ctx_t ctx = {0};
    err = esp_wireguard_init(&wg_config, &ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wireguard Connect init failed.");
        return;
    }

    /* start establishing the link. after this call, esp_wireguard start
    establishing connection. */
    err = esp_wireguard_connect(&ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wireguard Connect failed.");
        return;
    }

    // vTaskDelay(10000 /portTICK_PERIOD_MS);

    /* after some time, see if the link is up. note that it takes some time to
    establish the link */
    do {
        err = esp_wireguardif_peer_is_up(&ctx);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "WireGuard peer is up");
        }
        else {
            ESP_LOGI(TAG, "WireGuard peer is not up. %s",esp_err_to_name(err));
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    } while (err);
    esp_wireguard_set_default(&ctx);

}
