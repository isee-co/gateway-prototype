#ifndef H_MAIN_
#define H_MAIN_

#include "esp_log.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "nimble/ble.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_BLE = 0,
    EVENT_WIFI = 1,
    EVENT_MQTT = 2,
} event_type_t;

typedef enum {
    WIFI_INIT = 0,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_DISCONNECTED
} wifi_state_t;

typedef struct {
  uint8_t host[64];
  uint16_t port;
  uint8_t username[64];
  uint8_t password[256];
} mqtt_config_t;

typedef struct {
    uint8_t home_id[11];
    wifi_sta_config_t wifi;
    mqtt_config_t mqtt;
} app_config_t;

static app_config_t app_config;

static const char* event_type_str(event_type_t type) {
    switch (type) {
        case EVENT_BLE:
        return "BLE";
        case EVENT_WIFI:
        return "WIFI";
        case EVENT_MQTT:
        return "MQTT";
        default:
        return "UNKNOWN";
    }
}

static const char* wifi_state_str(wifi_state_t state) {
    switch (state) {
        case WIFI_INIT:
            return "INIT";
        case WIFI_CONNECTING:
            return "CONNECTING";
        case WIFI_CONNECTED:
            return "CONNECTED";
        case WIFI_DISCONNECTED:
            return "DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

esp_err_t cfg_init(void);
esp_err_t cfg_set_home_id(const char *id);
esp_err_t cfg_set_wifi_config(const char *ssid, const char *pass);
esp_err_t cfg_set_mqtt_config(const char *host, uint16_t port, const char *user, const char *pass);


esp_err_t ble_init(void);
void ble_notify_event(event_type_t type, const char *event);
esp_err_t ble_rpc_req_process(const char *json, char **response);

esp_err_t wifi_init(void);
esp_err_t wifi_connect(const char *ssid, const char *pass);

#ifdef __cplusplus
}
#endif

#endif