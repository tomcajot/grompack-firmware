#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_REGISTER(ble_simple_logger, LOG_LEVEL_DBG);

K_MSGQ_DEFINE(ble_data_queue, sizeof(struct neural_packet), 10, 4);

static struct neural_packet fake_packet;
static uint32_t fake_index = 0;

static void fake_data_handler(struct k_timer* dummy) {
    ARG_UNUSED(dummy);
    fake_packet.sample_index = fake_index++;
    for (int i = 0; i < PACKED_BUFFER_SIZE; i++) {
        fake_packet.packed_data[i] = (uint8_t)(i + fake_index);
    }
    k_msgq_put(&ble_data_queue, &fake_packet, K_NO_WAIT);
}
K_TIMER_DEFINE(fake_timer, fake_data_handler, NULL);

int main(void) {
    configure_ble();
    LOG_INF("BLE test running — waiting for subscription\n");

    struct neural_packet tx_packet;
    int err;

    while (1) {
        if (k_msgq_get(&ble_data_queue, &tx_packet, K_FOREVER) == 0) {
            if (is_laptop_subscribed) {
                err =
                    bt_nus_send(NULL, (uint8_t*)&tx_packet, sizeof(tx_packet));
                if (err && err != -EAGAIN && err != -ENOTCONN) {
                    LOG_ERR("Transmission error: %d\n", err);
                }
            }
        }
    }
}