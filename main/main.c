#include "main.h"

static const char *TAG = "APP";

void app_main(void)
{   
   ESP_ERROR_CHECK(cfg_init());
   ESP_ERROR_CHECK(ble_init());
   ESP_ERROR_CHECK(wifi_init());

    ESP_ERROR_CHECK(wifi_connect());
    while (wifi_state != WIFI_CONNECTED) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(mqtt_connect());    
    while (mqtt_state != MQTT_CONNECTED) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(zigbee_init());

}