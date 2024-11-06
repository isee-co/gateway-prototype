#include "string.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "main.h"

#define CONFIG_NAMESPASE "config"
#define APP_CONFIG_KEY "app_config"
#define B64_ID_LEN 11
#define WIFI_SSID_LEN 32
#define WIFI_PASS_LEN 64

static const char *TAG = "CFG";
app_config_t app_config = {0};

static esp_err_t cfg_load(void) {
    esp_err_t err;
    size_t length;
    nvs_handle_t nvs_handle;

    err = nvs_open(CONFIG_NAMESPASE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_get_blob(nvs_handle, APP_CONFIG_KEY, &app_config, &length);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "app config not found");
    } else {
        ESP_LOGI(TAG, "load app config. length=%d", length);
        ESP_LOGD(TAG, "app config: \n"
                 "\thome_id: %s\n"
                 "\twifi[ssid: %s, pass: %s]\n"
                 "\tmqtt[host: %s, port: %d, user: %s, pass: %s]\n",
                 app_config.home_id, app_config.wifi.ssid, app_config.wifi.password,
                 app_config.mqtt.host, app_config.mqtt.port, app_config.mqtt.username,
                 app_config.mqtt.password);              
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

static esp_err_t cfg_save(void) {
    esp_err_t err;
    nvs_handle_t nvs_handle;

    err = nvs_open(CONFIG_NAMESPASE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs_handle, APP_CONFIG_KEY, &app_config, sizeof(app_config));
    ESP_LOGI(TAG, "save app config");

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) return err;

    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t cfg_set_home_id(const char *id) {
    size_t len;
    esp_err_t err;

    len = strlen(id);
    if (len > B64_ID_LEN) {
        ESP_LOGD(TAG, "invalid id length: %s", id);
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy((char *)app_config.home_id, id, sizeof(app_config.home_id));

    err = cfg_save();
    ESP_RETURN_ON_ERROR(err, TAG, "save config failed.");

    return ESP_OK;
}

esp_err_t cfg_set_wifi_config(const char *ssid, const char *pass) {
    esp_err_t err;

    strlcpy((char *)app_config.wifi.ssid, ssid, sizeof(app_config.wifi.ssid));  
    strlcpy((char *)app_config.wifi.password, pass, sizeof(app_config.wifi.password));

    err = cfg_save();
    ESP_RETURN_ON_ERROR(err, TAG, "save config failed.");

    return ESP_OK;
}

esp_err_t cfg_set_mqtt_config(const char *host, uint16_t port, const char *user, const char *pass) {
    esp_err_t err;

    app_config.mqtt.port = port;
    strlcpy((char *)app_config.mqtt.host, host, sizeof(app_config.mqtt.host));
    strlcpy((char *)app_config.mqtt.username, user, sizeof(app_config.mqtt.username));
    strlcpy((char *)app_config.mqtt.password, pass, sizeof(app_config.mqtt.password));

    err = cfg_save();
    ESP_RETURN_ON_ERROR(err, TAG, "save config failed.");

    return ESP_OK;
}

esp_err_t cfg_init(void) {
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_flash_init failed. %d", err);

    err = cfg_load();
    ESP_RETURN_ON_ERROR(err, TAG, "read config failed.");

    return ESP_OK;
}