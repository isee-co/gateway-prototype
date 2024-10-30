#include "main.h"

static const char *TAG = "APP";

void app_main(void)
{   
   ESP_ERROR_CHECK(cfg_init());
   ESP_ERROR_CHECK(ble_init());
   ESP_ERROR_CHECK(wifi_init());
}