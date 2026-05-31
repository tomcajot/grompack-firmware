#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_logger, LOG_LEVEL_DBG);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

struct neural_packet {
    uint32_t sample_index;
    uint16_t ch1_data[120];
    uint16_t ch2_data[120];
} __packed;

K_MSGQ_DEFINE(ble_data_queue, sizeof(struct neural_packet), 10, 4);

static uint32_t global_index = 0;
static uint16_t simulated_voltage = 0;

static void adc_timer_handler(struct k_timer* dummy) {
    struct neural_packet new_data;
    new_data.sample_index = global_index++;

    int byte_idx = 0;

    for (int i = 0; i < 40; i++) {
        uint16_t ch1_samp_A = simulated_voltage;
        simulated_voltage = (simulated_voltage + 100) % 4096;
        uint16_t ch1_samp_B = simulated_voltage;
        simulated_voltage = (simulated_voltage + 100) % 4096;

        uint16_t ch2_samp_A = ch1_samp_A;
        uint16_t ch2_samp_B = ch1_samp_B;

        new_data.ch1_data[byte_idx] = (ch1_samp_A >> 4) & 0xFF;
        new_data.ch1_data[byte_idx + 1] =
            ((ch1_samp_A & 0x0F) << 4) | ((ch1_samp_B >> 8) & 0x0F);
        new_data.ch1_data[byte_idx + 2] = ch1_samp_B & 0xFF;

        new_data.ch2_data[byte_idx] = (ch2_samp_A >> 4) & 0xFF;
        new_data.ch2_data[byte_idx + 1] =
            ((ch2_samp_A & 0x0F) << 4) | ((ch2_samp_B >> 8) & 0x0F);
        new_data.ch2_data[byte_idx + 2] = ch2_samp_B & 0xFF;

        byte_idx += 3;
    }

    k_msgq_put(&ble_data_queue, &new_data, K_NO_WAIT);
}
K_TIMER_DEFINE(adc_timer, adc_timer_handler, NULL);

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

    if (enabled) {
        k_timer_start(&adc_timer, K_MSEC(8), K_MSEC(8));
    } else {
        k_timer_stop(&adc_timer);
    }
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

int main(void) {
    int err;

    LOG_INF("Booting grompack engine for connection...\n");

    err = bt_nus_cb_register(&nus_listener, NULL);
    if (err) return err;

    err = bt_enable(NULL);
    if (err) return err;

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd,
                          ARRAY_SIZE(sd));
    if (err) return err;

    LOG_INF("Advertising as '%s'. Ready for connection.\n", DEVICE_NAME);

    struct neural_packet tx_packet;

    while (true) {
        LOG_INF("in loop");
        if (k_msgq_get(&ble_data_queue, &tx_packet, K_FOREVER) == 0) {
            err = bt_nus_send(NULL, (uint8_t*)&tx_packet, sizeof(tx_packet));

            if (err && err != -EAGAIN && err != -ENOTCONN) {
                LOG_ERR("Transmission error: %d\n", err);
            }
        }
    }

    return 0;
}