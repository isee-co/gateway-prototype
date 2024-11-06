#include "cJSON.h"

#include "main.h"

#define M_SET_HOME_ID "SetHomeId"
#define M_SET_MQTT_CONFIG "SetMqttConfig"
#define M_SET_WIFI_CONFIG "SetWifiConfig"

static const char *TAG = "BleRpc";

esp_err_t rpc_set_wifi_config(cJSON *params, char **result) {
    esp_err_t err;
    char *ssid, *pass;

    if (cJSON_GetObjectItem(params, "ssid")) {
        ssid = cJSON_GetObjectItem(params, "ssid")->valuestring;
    } else {
        *result = "ssid is required";
        return ESP_ERR_INVALID_ARG;
    }

    if (cJSON_GetObjectItem(params, "pass")) {
        pass = cJSON_GetObjectItem(params, "pass")->valuestring;
    } else {
        *result = "password is required";
        return ESP_ERR_INVALID_ARG;
    }
    err = cfg_set_wifi_config(ssid, pass);
    if (err != ESP_OK) {
        *result = "set wifi config failed";
        return err;
    }
    
    err = wifi_connect();
    if (err != ESP_OK) {
        *result = "wifi connecting failed.";
        return err;
    }

    *result = NULL;
    return ESP_OK;
}

esp_err_t rpc_set_home_id(cJSON *params, char **result) {
    esp_err_t err;
    
    err = cfg_set_home_id(params->valuestring);
    if (err != ESP_OK) {
        *result = "set home id failed";
        return err;
    }

    *result = NULL;
    return ESP_OK;
}

esp_err_t rpc_set_mqtt_config(cJSON *params, char **result) {
  esp_err_t err;
  cJSON *host, *port, *user, *pass;

    host = cJSON_GetObjectItem(params, "host");
    port = cJSON_GetObjectItem(params, "port");
    user = cJSON_GetObjectItem(params, "user");
    pass = cJSON_GetObjectItem(params, "pass");

  if (host == NULL || port == NULL || user == NULL || pass == NULL) {
    *result = "request format is invalid.";
    return ESP_ERR_INVALID_ARG;
  }

    err = cfg_set_mqtt_config(host->valuestring, port->valueint, user->valuestring,
                              pass->valuestring);
  if (err != ESP_OK) {
    *result = "set mqtt config failed";
    return err;
  }

    err = mqtt_connect();
    if (err != ESP_OK) {
        *result = "mqtt connect failed";
        return err;
    }

  *result = NULL;
  return ESP_OK;
}

esp_err_t ble_rpc_req_process(const char *json, char **response) {
    esp_err_t err;
    char *result;
    cJSON *req = cJSON_Parse(json);
    cJSON *res = cJSON_CreateObject();

    if (req == NULL) {
        ESP_LOGW(TAG, "request json parse failed: %s", json);
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *id = cJSON_GetObjectItem(req, "id");
    cJSON *method = cJSON_GetObjectItem(req, "method");
    cJSON *params = cJSON_GetObjectItem(req, "payload");
    if (id == NULL || method == NULL || params == NULL) {
        ESP_LOGE(TAG, "request format is invalid.");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(method->valuestring, M_SET_HOME_ID) == 0) {
        err = rpc_set_home_id(params, &result);
    } else if (strcmp(method->valuestring, M_SET_WIFI_CONFIG) == 0) {
        err = rpc_set_wifi_config(params, &result);
    } else if (strcmp(method->valuestring, M_SET_MQTT_CONFIG) == 0) {
        err = rpc_set_mqtt_config(params, &result);
    } else {
        err = ESP_ERR_NOT_FOUND;
        result = "method not found";
    }

    cJSON_AddNumberToObject(res, "id", id->valueint);
    if (err != ESP_OK) {
        cJSON_AddFalseToObject(res, "success");
        cJSON_AddStringToObject(res, "error", result);
    } else {
        cJSON_AddTrueToObject(res, "success");
        if (result != NULL) {
            cJSON_AddStringToObject(res, "data", result);
        }
    }

    *response = cJSON_PrintUnformatted(res);
    cJSON_Delete(req);
    cJSON_Delete(res);

    return ESP_OK;
}
