#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>

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
        LOG_INF("BLE init failed (err %d)\n", err);
        status_flag = ERROR;
        return;
    }

    err = bt_nus_cb_register(&nus_listener, NULL);
    if (err) {
        LOG_INF("NUS callbacks failed to register (err %d)\n", err);
        status_flag = ERROR;
        return;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd,
                          ARRAY_SIZE(sd));
    if (err) {
        LOG_INF("Advertising failed to start (err %d)\n", err);
        status_flag = ERROR;
        return;
    }

    LOG_INF("Bluetooth initialized and advertising successfully.\n");
}