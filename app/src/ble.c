#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

bool is_laptop_subscribed = false;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void notif_enabled(bool enabled, void* ctx) {
    ARG_UNUSED(ctx);
    LOG_INF("Laptop Subscription: %s\n", (enabled ? "Enabled" : "Disabled"));

    is_laptop_subscribed = enabled;

    k_msgq_purge(&ble_data_queue);
}

static void received(struct bt_conn* conn, const void* data, uint16_t len,
                     void* ctx) {
    ARG_UNUSED(conn);
    ARG_UNUSED(ctx);
    LOG_INF("Command Received from Laptop: %.*s\n", len, (char*)data);
}

struct bt_nus_cb nus_listener = {
    .notif_enabled = notif_enabled,
    .received = received,
};

void configure_ble(void) {
    int err;

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("BLE init failed (err %d)\n", err);
        return;
    }

    err = bt_nus_cb_register(&nus_listener, NULL);
    if (err) {
        LOG_ERR("NUS callbacks failed to register (err %d)\n", err);
        return;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd,
                          ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)\n", err);
        return;
    }

    LOG_INF("Bluetooth initialized and advertising successfully.\n");
}

static void on_connected(struct bt_conn* conn, uint8_t err) {
    if (err) {
        LOG_ERR("Connection failed, err 0x%02x\n", err);
        return;
    }
    LOG_INF("Connected!\n");

    bt_conn_le_data_len_update(conn, BT_LE_DATA_LEN_PARAM_MAX);
    bt_conn_le_phy_update(conn, BT_CONN_LE_PHY_PARAM_2M);
}

static void on_disconnected(struct bt_conn* conn, uint8_t reason) {
    LOG_INF("Disconnected, reason 0x%02x\n", reason);

    is_laptop_subscribed = false;
    k_msgq_purge(&ble_data_queue);

    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd,
                              ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Failed to restart advertising (err %d)\n", err);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};