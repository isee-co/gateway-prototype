#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "console/console.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "../src/ble_hs_hci_priv.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "main.h"

#define MIN_REQUIRED_MBUF 2 /* Assuming payload of 500Bytes and each mbuf can take 292Bytes.  */
#define PREFERRED_MTU_VALUE 512
#define LL_PACKET_TIME 2120
#define LL_PACKET_LENGTH 251
#define MTU_DEF 512

#define CONFIG_SVC_UID 0x5501
#define EVENTS_CHR_UID 0xCC02
#define JSON_RPC_CHR_UID 0xCC03



static const char *TAG = "BLE";
static const char *DAVICE_NAME = "Zigbee-Gateway";

static uint16_t conn_handle;
static uint8_t gatts_addr_type;
static uint16_t events_handel;
static bool event_char_subscribed = false;
static uint16_t json_rpc_handel;
static bool rpc_char_subscribed = false;

static int gatts_gap_event(struct ble_gap_event *event, void *arg);
static int gatts_char_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);                        

static const struct ble_gatt_svc_def gatts_config_svcs[] = {
    {
        /*** Gateway configuration serviec */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(CONFIG_SVC_UID),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    // events notify characteristic
                    .uuid = BLE_UUID16_DECLARE(EVENTS_CHR_UID),
                    .access_cb = gatts_char_access_cb,
                    .val_handle = &events_handel,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    // json rpc characteristic
                    .uuid = BLE_UUID16_DECLARE(JSON_RPC_CHR_UID),
                    .access_cb = gatts_char_access_cb,
                    .val_handle = &json_rpc_handel,
                    .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_WRITE,
                },
                {0, /* No more characteristics in this service. */},
            },
    },
    {0, /* No more services. */},
};

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

esp_err_t ble_notify_attr(uint16_t attr_handle, uint8_t *data, uint16_t len) {
    struct os_mbuf *om;
    om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        /* Memory not available for mbuf */
        ESP_LOGW(TAG, "No MBUFs available from pool, retry..");
        return ESP_ERR_NO_MEM;
    }

    return ble_gatts_notify_custom(conn_handle, attr_handle, om);
}

void ble_notify_event(event_type_t type, const char *event) {
    if (!event_char_subscribed) return;

    int len;
    char eventBuf[200];
    len = sprintf(eventBuf, "{\"type\":\"%s\" \"event\":\"%s\"}", event_type_str(type), event);

    esp_err_t err = ble_notify_attr(events_handel, (uint8_t *)eventBuf, len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ble notify event failed. event: %.*s err: %d", len, eventBuf, err);
    }
    
    ESP_LOGD(TAG, "notify event: %.*s", len, eventBuf);
}

static int rpc_response_send(char *res_buf) {
    esp_err_t err;
    if (!rpc_char_subscribed) {
        return ESP_ERR_INVALID_STATE;
    }

    err = ble_notify_attr(json_rpc_handel, (uint8_t *)res_buf, strlen(res_buf));
    ESP_RETURN_ON_ERROR(err, TAG, "ble notify wifi status failed. %d", err);

    return ESP_OK;
}

static int gatts_char_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);
    switch (uuid) {
        case EVENTS_CHR_UID:
            return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
        case JSON_RPC_CHR_UID:
            if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
                esp_err_t err;
                size_t om_len = OS_MBUF_PKTLEN(ctxt->om);
                char req_buf[om_len + 1];
                memset(req_buf, '\0', om_len + 1);

                err = ble_hs_mbuf_to_flat(ctxt->om, req_buf, om_len, NULL);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "read wifi config failed.");
                    return BLE_ATT_ERR_UNLIKELY;
                }

                char *res_buf;
                ESP_LOGD(TAG, "rpc request: %s", req_buf);
                err = ble_rpc_req_process(req_buf, &res_buf);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "set wifi config failed. error=%d", err);
                    return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
                }

                if (res_buf) {
                    ESP_LOGD(TAG, "rpc response: %s", res_buf);
                    rpc_response_send(res_buf);
                }

                return ESP_OK;
            }

            return BLE_ATT_ERR_REQ_NOT_SUPPORTED;            

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

/*
 * Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
static void gatts_advertise(void) {
    esp_err_t err;
    struct ble_hs_adv_fields fields;
    struct ble_gap_adv_params adv_params;

    memset(&fields, 0, sizeof(fields));

    /*
     * Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t *)DAVICE_NAME;
    fields.name_len = strlen(DAVICE_NAME);
    fields.name_is_complete = 1;

    err = ble_gap_adv_set_fields(&fields);
    if (err != 0) {
        ESP_LOGE(TAG, "Error setting advertisement data; err=%d", err);
        return;
    }

    /* Begin advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    err = ble_gap_adv_start(gatts_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gatts_gap_event,
                            NULL);
    if (err != 0) {
        ESP_LOGE(TAG, "Error enabling advertisement; err=%d", err);
        return;
    }
}

static int gatts_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;
    int err;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            /* A new connection was established or a connection attempt failed */
            ESP_LOGI(TAG, "connection %s; status = %d ",
                     event->connect.status == 0 ? "established" : "failed", event->connect.status);
            err = ble_att_set_preferred_mtu(PREFERRED_MTU_VALUE);
            if (err != 0) {
                ESP_LOGE(TAG, "Failed to set preferred MTU; err = %d", err);
            }

            if (event->connect.status != 0) {
                /* Connection failed; resume advertising */
                gatts_advertise();
            }

            err = ble_hs_hci_util_set_data_len(event->connect.conn_handle, LL_PACKET_LENGTH,
                                               LL_PACKET_TIME);
            if (err != 0) {
                ESP_LOGE(TAG, "Set packet length failed");
            }

            conn_handle = event->connect.conn_handle;
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "disconnect; reason = %d", event->disconnect.reason);
            rpc_char_subscribed = false;
            event_char_subscribed = false;

            gatts_advertise();
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            /* The central has updated the connection parameters. */
            ESP_LOGI(TAG, "connection updated; status=%d ", event->conn_update.status);
            err = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            assert(err == 0);
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "adv complete ");
            gatts_advertise();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG,
                     "subscribe event; cur_notify=%d; value handle; "
                     "val_handle = %d",
                     event->subscribe.cur_notify, event->subscribe.attr_handle);

            if (event->subscribe.attr_handle == json_rpc_handel) {
                rpc_char_subscribed = event->subscribe.cur_notify;
            } else if (event->subscribe.attr_handle == events_handel) {
                event_char_subscribed = event->subscribe.cur_notify;
            }
            break;

        case BLE_GAP_EVENT_NOTIFY_TX:
            ESP_LOGD(TAG, "BLE_GAP_EVENT_NOTIFY_TX success !!");
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "mtu update event; conn_handle = %d mtu = %d ", event->mtu.conn_handle,
                     event->mtu.value);
            break;
    }
    return 0;
}

static void gatts_on_sync(void) {
    int err;
    err = ble_hs_id_infer_auto(0, &gatts_addr_type);
    assert(err == 0);

    /* Begin advertising */
    gatts_advertise();
}

static void gatts_on_reset(int reason) { ESP_LOGE(TAG, "Resetting state; reason=%d", reason); }

void gatts_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");

    nimble_port_run();
    nimble_port_freertos_deinit();
}

int ble_init(void) {
    esp_err_t err;
    err = nimble_port_init();
    ESP_RETURN_ON_ERROR(err, TAG, "nimble_port_init failed. %d", err);

    /* Initialize the NimBLE host configuration */
    ble_hs_cfg.sync_cb = gatts_on_sync;
    ble_hs_cfg.reset_cb = gatts_on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    err = ble_gatts_count_cfg(gatts_config_svcs);
    ESP_RETURN_ON_ERROR(err, TAG, "ble_gatts_count_cfg failed. %d", err);

    err = ble_gatts_add_svcs(gatts_config_svcs);
    ESP_RETURN_ON_ERROR(err, TAG, "ble_gatts_add_svcs failed. %d", err);

    /* Set the default device name */
    err = ble_svc_gap_device_name_set(DAVICE_NAME);
    ESP_RETURN_ON_ERROR(err, TAG, "ble_svc_gap_device_name_set failed. %d", err);

    /* Start the task */
    nimble_port_freertos_init(gatts_host_task);

    return ESP_OK;
}
