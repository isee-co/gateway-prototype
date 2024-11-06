#include "main.h"

static const char *TAG = "APP";

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("APP", ESP_LOG_VERBOSE);
    esp_log_level_set("CFG", ESP_LOG_VERBOSE);
    esp_log_level_set("BLE", ESP_LOG_VERBOSE);
    esp_log_level_set("WIFI", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT", ESP_LOG_VERBOSE);
    esp_log_level_set("ZIGBEE", ESP_LOG_VERBOSE);
    esp_log_level_set("BleRpc", ESP_LOG_VERBOSE);
    esp_log_level_set("MqttRpc", ESP_LOG_VERBOSE);

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