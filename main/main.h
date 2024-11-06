#ifndef H_MAIN_
#define H_MAIN_

#include "esp_log.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "nimble/ble.h"

#ifdef __cplusplus
extern "C" {
#endif

static const uint8_t GATEWAY_ID[8] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x01 };
static const char *GATEWAY_ID_B64 = "qrvM3e7_AAE"; /*url base64 of GATEWAY_ID*/

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

typedef enum {
    MQTT_CONNECTING = 0,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED
} mqtt_state_t;

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

typedef enum {
    DEVICE_ON_OFF = 0x06,
    DEVICE_DIMMER = 0x02,
    DEVICE_WINDOW_COVER = 0x03,
    DEVICE_AC_CONTROLLER = 0x04,
} endpoint_type_t;

typedef struct {
    uint8_t id;
    endpoint_type_t type;
    char *disc;
} endpoint_info_t;

typedef struct {
    uint8_t ieee_addr[8];
    uint8_t ep_count;
    endpoint_info_t ep_list[];
} device_info_t;

extern app_config_t app_config;
extern wifi_state_t wifi_state;
extern mqtt_state_t mqtt_state;

esp_err_t cfg_init(void);
esp_err_t cfg_set_home_id(const char *id);
esp_err_t cfg_set_wifi_config(const char *ssid, const char *pass);
esp_err_t cfg_set_mqtt_config(const char *host, uint16_t port, const char *user, const char *pass);


esp_err_t ble_init(void);
void ble_notify_event(event_type_t type, const char *event);
esp_err_t ble_rpc_req_process(const char *json, char **response);

esp_err_t wifi_init(void);
esp_err_t wifi_connect(const char *ssid, const char *pass);

esp_err_t zigbee_init(void);
esp_err_t zigbee_join_permit(bool enable, uint8_t duration);
void zigbee_set_on_off_state(uint8_t *ieee_addr, uint8_t endpoint, bool state);

esp_err_t mqtt_connect(void);
esp_err_t mqtt_rpc_req_process(const char *json, char **response);
void mqtt_publish_dev_state(uint8_t ieee_addr[8], uint8_t endpoint, uint16_t cluster, void *value);
void mqtt_publish_dev_annnc(device_info_t *dev_info);

#ifdef __cplusplus
}
#endif

#endif