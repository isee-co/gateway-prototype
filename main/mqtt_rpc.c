#include "cJSON.h"
#include "../src/utils/base64.h"

#include "main.h"

#define M_SET_JOIN_PERMIT "SetJoinPermit"
#define M_SET_DEVICE_STATE "SetDeviceState"
#define DEFAULT_JOIN_PERMIT_DURATION 10

static const char *TAG = "MqttRpc";

static esp_err_t rpc_set_join_permit(cJSON *params, char **result) {
    esp_err_t err;
    bool enable = false;
    uint8_t duration = DEFAULT_JOIN_PERMIT_DURATION;

    if (cJSON_GetObjectItem(params, "state")) {
        char *state = cJSON_GetObjectItem(params, "state")->valuestring;
        if (strcmp(state, "enable") == 0) {
            enable = true;
        } else if (strcmp(state, "disable") == 0) {
            enable = false;
        } else {
            *result = "invalid state value";
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        *result = "state is requred.";
        return ESP_ERR_INVALID_ARG;         
    }

    if (cJSON_GetObjectItem(params, "timeout")) {
        duration = cJSON_GetObjectItem(params, "timeout")->valueint;
    }

    err = zigbee_join_permit(enable, duration);
    if (err != ESP_OK) {
        *result = "set join permit failed";
        return err;
    }

    ESP_LOGD(TAG, "network %s successfuly.", enable ? "opened" : "closed");
    cJSON *res = cJSON_CreateObject();
    cJSON_AddTrueToObject(res, "success");
    cJSON_AddNumberToObject(res, "timeout", duration);
    *result = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);

    return ESP_OK;
}

static esp_err_t rpc_set_device_state(cJSON *params, char **result) {
    esp_err_t err;
    cJSON *device_id, *endpoint, *type, *state; 
    uint8_t *ieee_addr;

    device_id =  cJSON_GetObjectItem(params, "device_id");
    endpoint = cJSON_GetObjectItem(params, "endpoint");
    state = cJSON_GetObjectItem(params, "state");
    type =  cJSON_GetObjectItem(params, "type");

    size_t out_len;
    ieee_addr = base64_url_decode(device_id->valuestring, 11, &out_len);
    if (out_len != 8){
        *result = "device id in not valid.";
        return ESP_ERR_INVALID_ARG;        
    }
    

    if (!device_id || !endpoint || !type || !state){
        *result = "parameters in not valid.";
        return ESP_ERR_INVALID_ARG;
    }

    switch (type->valueint) {
        case DEVICE_ON_OFF:
            bool on_off = cJSON_GetObjectItem(state, "onOff")->valueint;
            zigbee_set_on_off_state(ieee_addr, endpoint->valueint, on_off);
            break;
        default:
            *result = "device type is invalid.";
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t mqtt_rpc_req_process(const char *json, char **response) {
    esp_err_t err;
    char *result;
    cJSON *req = cJSON_Parse(json);
    cJSON *res = cJSON_CreateObject();

    if (req == NULL) {
        ESP_LOGW(TAG, "request json parse failed: %s", json);
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *method = cJSON_GetObjectItem(req, "method");
    cJSON *params = cJSON_GetObjectItem(req, "params");
    if (method == NULL || params == NULL) {
        ESP_LOGE(TAG, "request format is invalid.");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(method->valuestring, M_SET_JOIN_PERMIT) == 0) {
        err = rpc_set_join_permit(params, &result);
    } else if (strcmp(method->valuestring, M_SET_DEVICE_STATE) == 0) {
        err = rpc_set_device_state(params, &result);
    } else {
        err = ESP_ERR_NOT_FOUND;
        result = "method not found";
    }

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