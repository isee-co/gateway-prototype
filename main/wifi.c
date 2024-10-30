#include "string.h"
#include "esp_wifi.h"

#include "main.h"

static const char* TAG = "WIFI";

static wifi_state_t wifi_state;

static void update_wifi_state(wifi_state_t state) {
    wifi_state = state;
    ble_notify_event(EVENT_WIFI, wifi_state_str(state));
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                          void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        update_wifi_state(WIFI_CONNECTING);
        ESP_LOGI(TAG, "wifi connecting to the AP");
        // esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        update_wifi_state(WIFI_DISCONNECTED);
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        update_wifi_state(WIFI_CONNECTED);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_connect_task(void* config) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_wifi_connect();

    vTaskDelete(NULL);
}

esp_err_t wifi_connect(const char* ssid, const char* pass) {
    esp_err_t err;
    wifi_config_t wifi_config = {0};

    if (!(ssid && pass)) {
        ESP_LOGE(TAG, "wifi connecting failed. ssid and pass is required");
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_RETURN_ON_ERROR(err, TAG, "wifi set mode failed. %d", err);

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    ESP_RETURN_ON_ERROR(err, TAG, "wifi set config failed. %d", err);
    
    update_wifi_state(WIFI_CONNECTING);
    // err = esp_wifi_connect();
    xTaskCreate(wifi_connect_task, "wifi_connect_task", 4*1024, NULL, 5, NULL);

    return ESP_OK;
}



esp_err_t wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    update_wifi_state(WIFI_INIT);
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    return ESP_OK;
}