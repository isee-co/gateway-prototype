#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"

#include "main.h"

/* Zigbee configuration */
#define MAX_CHILDREN 10                 /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define HA_ONOFF_SWITCH_ENDPOINT 1      /* esp light switch device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK (1l << 13) /* Zigbee primary channel mask use in the example*/

static const char *TAG = "ZIGBEE";

esp_err_t zigbee_join_permit(bool enable, uint8_t duration) {
    if (enable) {
        return esp_zb_bdb_open_network(duration);
    } else {
        return esp_zb_bdb_close_network();
    }
}

void zigbee_set_on_off_state(uint8_t *ieee_addr, uint8_t endpoint, bool state) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(cmd_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, 8);
    if (state) {
        cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
    } else {
        cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
    }

    ESP_EARLY_LOGI(TAG, "Send 'on/off' command to endpoint %d", endpoint);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
}

static void find_on_off_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint,
                         void *user_ctx) {
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Found light: addr(0x%04hx) on endpoint(%d)", addr, endpoint);
        
        device_info_t *dev_info = malloc(sizeof(device_info_t) + sizeof(endpoint_info_t) * 1);
        esp_zb_ieee_address_by_short(addr, dev_info->ieee_addr);        
        dev_info->ep_count = 1;
        dev_info->ep_list[0].id = endpoint;
        dev_info->ep_list[0].type = 0x06;
        dev_info->ep_list[0].disc = "On/Off Switch";
        mqtt_publish_dev_annnc(dev_info);
    }
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;
    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Zigbee stack initialized");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Device started up in %s factory-reset mode",
                         esp_zb_bdb_is_factory_new() ? "" : "non");
                if (esp_zb_bdb_is_factory_new()) {
                    ESP_LOGI(TAG, "Start network formation");
                    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
                } else {
                    ESP_LOGI(TAG, "Open network for starting");
                    esp_zb_bdb_open_network(10);
                }
            } else {
                ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)",
                         esp_err_to_name(err_status));
            }
            break;
        case ESP_ZB_BDB_SIGNAL_FORMATION:
            if (err_status == ESP_OK) {
                esp_zb_ieee_addr_t extended_pan_id;
                esp_zb_get_extended_pan_id(extended_pan_id);
                ESP_LOGI(TAG,
                         "Formed network successfully (Extended PAN ID: "
                         "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, "
                         "Short Address: 0x%04hx)",
                         extended_pan_id[7], extended_pan_id[6], extended_pan_id[5],
                         extended_pan_id[4], extended_pan_id[3], extended_pan_id[2],
                         extended_pan_id[1], extended_pan_id[0], esp_zb_get_pan_id(),
                         esp_zb_get_current_channel(), esp_zb_get_short_address());
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Restart network formation (status: %s)",
                         esp_err_to_name(err_status));
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                       ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
            }
            break;
        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Network steering started");
            }
            break;
        case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
            dev_annce_params =
                (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
            ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)",
                     dev_annce_params->device_short_addr);
            esp_zb_zdo_match_desc_req_param_t cmd_req;
            cmd_req.dst_nwk_addr = dev_annce_params->device_short_addr;
            cmd_req.addr_of_interest = dev_annce_params->device_short_addr;
            esp_zb_zdo_find_on_off_light(&cmd_req, find_on_off_cb, NULL);
            break;
        case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
            if (err_status == ESP_OK) {
                if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                    ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(),
                             *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
                } else {
                    ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.",
                             esp_zb_get_pan_id());
                }
            }
            break;
        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s",
                     esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
            break;
    }
}

static esp_err_t zb_attribute_report_handler(const esp_zb_zcl_report_attr_message_t *message) {
    esp_err_t ret = ESP_OK;
    bool light_state = false;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                        "Report attribute: error status(%d)", message->status);
    ESP_LOGI(TAG,
             "Report attribute: endpoint(%d), cluster(0x%x), attribute(0x%x),"
             " data: size(%d) type(%02x)",
             message->src_endpoint, message->cluster, message->attribute.id,
             message->attribute.data.size, message->attribute.data.type);

    uint8_t ieee_addr[8];
    if (message->src_address.addr_type == ESP_ZB_ZCL_ADDR_TYPE_SHORT) {
        esp_zb_ieee_address_by_short(message->src_address.u.short_addr, ieee_addr);
        ESP_LOGV(TAG, "get ieee_addr by short addr, %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 ieee_addr[7], ieee_addr[6], ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2],
                 ieee_addr[1], ieee_addr[0]);
    } else {
        memcpy(ieee_addr, message->src_address.u.ieee_addr, 8);
    }

    mqtt_publish_dev_state(ieee_addr, message->src_endpoint, message->cluster,
                           message->attribute.data.value);

    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id,
                                   const void *message) {
    esp_err_t ret = ESP_OK;

    switch (callback_id) {
        case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
            ESP_LOGD(TAG, "Received action: ESP_ZB_CORE_REPORT_ATTR_CB_ID");
            ret = zb_attribute_report_handler((const esp_zb_zcl_report_attr_message_t *)message);
            break;

        default:
            ESP_LOGW(TAG, "Unhandled action: %d", callback_id);
            break;
    }

    return ret;
}

static void esp_zb_task(void *pvParameters) {
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,
        .nwk_cfg.zczr_cfg = {.max_children = MAX_CHILDREN},
    };

    /* initialize Zigbee stack */
    esp_zb_init(&zb_nwk_cfg);

    esp_zb_on_off_switch_cfg_t switch_cfg = ESP_ZB_DEFAULT_ON_OFF_SWITCH_CONFIG();
    esp_zb_ep_list_t *switch_ep =
        esp_zb_on_off_switch_ep_create(HA_ONOFF_SWITCH_ENDPOINT, &switch_cfg);
    esp_zb_device_register(switch_ep);

    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    // esp_zb_nvram_erase_at_start(true);

    ESP_ERROR_CHECK(esp_zb_start(false));

    esp_zb_main_loop_iteration();
}

esp_err_t zigbee_init(void) {
    esp_zb_platform_config_t config = {
        .radio_config = {.radio_mode = RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = HOST_CONNECTION_MODE_NONE},
    };

    esp_err_t err = (esp_zb_platform_config(&config));
    ESP_RETURN_ON_ERROR(err, TAG, "Zigbee platform config failed. %d", err);

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

    return ESP_OK;
}