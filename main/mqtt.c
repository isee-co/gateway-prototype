#include "mqtt_client.h"
#include "sys/time.h"
#include "cJSON.h"
#include "../src/utils/base64.h"
#include "main.h"

#define RPC_REQ "RPCREQ"
#define RPC_RES "RPCRES"
#define DEV_STATE "DEVSTATE"
#define DEV_JOIN "DEVJOIN"

static const char *TAG = "MQTT";
mqtt_state_t mqtt_state = MQTT_DISCONNECTED;
esp_mqtt_client_handle_t client;

static const char *mqtt_state_str(mqtt_state_t state) {
    switch (state) {
        case MQTT_CONNECTING:
            return "CONNECTING";
        case MQTT_CONNECTED:
            return "CONNECTED";
        case MQTT_DISCONNECTED:
            return "DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

static void update_mqtt_state(mqtt_state_t state) {
    mqtt_state = state;
    ble_notify_event(EVENT_MQTT, mqtt_state_str(state));
}

void mqtt_publish_dev_state(uint8_t ieee_addr[8], uint8_t endpoint, uint16_t cluster, void *value) {
    char topic[50];
    size_t out_len;
    char *dev_id = base64_url_encode(ieee_addr, 8, &out_len);
    snprintf(topic, sizeof(topic), "/%s/%s/%s/%d", app_config.home_id, DEV_STATE, dev_id, endpoint);
    
    cJSON *event = cJSON_CreateObject();
    cJSON_AddNumberToObject(event, "timestamp", esp_timer_get_time());
    cJSON_AddNumberToObject(event, "type", cluster);
    cJSON *state = cJSON_AddObjectToObject(event, "state");

    switch (cluster) {
        case DEVICE_ON_OFF:
            if (*(bool *)value){
                cJSON_AddTrueToObject(state, "onOff");
            } else {
                cJSON_AddFalseToObject(state, "onOff");
            }
            break;
        default:
            ESP_LOGW(TAG, "this cluster (%02x) not support yet.", cluster);
            return;
    }

    char *event_str = cJSON_PrintUnformatted(event);
    esp_mqtt_client_publish(client, topic, event_str, 0, 1, 0);

    cJSON_Delete(event);
    cJSON_free(event_str);
}

void mqtt_publish_dev_annnc(device_info_t *dev_info) {
    char topic[50];
    size_t out_len;
    char *dev_id = base64_url_encode(dev_info->ieee_addr, 8, &out_len);
    snprintf(topic, sizeof(topic), "/%s/%s/%s", app_config.home_id, DEV_JOIN, GATEWAY_ID_B64);
    
    cJSON *event = cJSON_CreateObject();
    cJSON_AddStringToObject(event, "device_id", dev_id);
    cJSON_AddStringToObject(event, "device_info", "{}");
    cJSON *endpoints = cJSON_AddObjectToObject(event, "endpoints");
    for (int i = 0; i < dev_info->ep_count; i++) {        
        cJSON_AddNumberToObject(endpoints, "id", dev_info->ep_list[i].id);
        cJSON_AddNumberToObject(endpoints, "type", dev_info->ep_list[i].type);
        cJSON_AddStringToObject(endpoints, "description", dev_info->ep_list[i].disc);
    }
    
    char *event_str = cJSON_PrintUnformatted(event);
    esp_mqtt_client_publish(client, topic, event_str, 0, 1, 0);

    cJSON_Delete(event);
    cJSON_free(event_str);
}

static void mqtt_connected(esp_mqtt_client_handle_t client) {
    char topic[50];
    // publish gateway status
    snprintf(topic, sizeof(topic), "/%s/EVENTS/%s/STATUS", app_config.home_id, GATEWAY_ID_B64);
    esp_mqtt_client_publish(client, topic, "online", 0, 1, 1);
    
    // subscribe rpc request topic
    snprintf(topic, sizeof(topic), "/%s/%s/%s", app_config.home_id, RPC_REQ, GATEWAY_ID_B64);
    esp_mqtt_client_subscribe(client, topic, 1);
}

static void mqtt_rpc_meg_handler(esp_mqtt_event_t *event) {
    esp_err_t err;
    char *response;

    err = mqtt_rpc_req_process(event->data, &response);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rpc request process failed. error=%d", err);
        return;
    }

    if (response) {
        char topic[32];
        char *requster_id = event->property->response_topic;
        esp_mqtt5_publish_property_config_t property = {
            .correlation_data = event->property->correlation_data,
            .correlation_data_len = event->property->correlation_data_len,
        };
        snprintf(topic, sizeof(topic), "/%s/%s/%s", app_config.home_id, RPC_RES, requster_id);
        ESP_LOGD(TAG, "rpc send response: %s to topic: %s", response, topic);
        esp_mqtt5_client_set_publish_property(event->client, &property);
        esp_mqtt_client_publish(event->client, topic, response, 0, 1, 0);

        free(response);
    }
}

static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                               void *event_data) {
    esp_mqtt_event_t *event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            update_mqtt_state(MQTT_CONNECTED);
            mqtt_connected(event->client);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            update_mqtt_state(MQTT_DISCONNECTED);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
            // todo: check if topic is rpc request
            mqtt_rpc_meg_handler(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            esp_mqtt_error_codes_t *err = event->error_handle;
            ESP_LOGI(TAG, "MQTT5 return code is %d", err->connect_return_code);
            if (err->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", err->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", err->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",
                                     err->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(err->esp_transport_sock_errno));
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

esp_err_t mqtt_connect() {
    esp_err_t err;
    mqtt_config_t *cfg = &app_config.mqtt;
    char lastwill_topic[50];
    snprintf(lastwill_topic, sizeof(lastwill_topic), "/%s/EVENTS/%s/STATUS", app_config.home_id, GATEWAY_ID_B64);
    esp_mqtt_client_config_t mqtt5_cfg = {
        .network.disable_auto_reconnect = true,
        .session.protocol_ver = MQTT_PROTOCOL_V_5,
        .broker.address = {.port = cfg->port,
                           .hostname = (char *)cfg->host,
                           .transport = MQTT_TRANSPORT_OVER_TCP},
        .credentials = {.username = (char *)cfg->username,
                        .authentication.password = (char *)cfg->password},
        .session.last_will =
            {
                .topic = lastwill_topic,
                .msg = "offline",
                .msg_len = 7,
                .retain = true,
            },
    };

    client = esp_mqtt_client_init(&mqtt5_cfg);

    err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_RETURN_ON_ERROR(err, TAG, "register event failed. %d", err);

    ESP_LOGI(TAG, "mqtt client start. with host=%s:%d", cfg->host, cfg->port);

    update_mqtt_state(MQTT_CONNECTING);

    err = esp_mqtt_client_start(client);
    ESP_RETURN_ON_ERROR(err, TAG, "mqtt conncting fiald. %d", err);

    return ESP_OK;
}