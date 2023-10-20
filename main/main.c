#include <stdio.h>
#include "communication.h"
#include "modem.h"
#include "wireguard.h"
#include "sync_time.h"
#include <nvs_flash.h>

void app_main(void)
{
    nvs_flash_init();
    init_modem();
    obtain_time();
    init_wireguard();
    communication_module();
}