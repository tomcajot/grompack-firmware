#include <zephyr/kernel.h>

#include "peripherals.h"

// LOG_MODULE_REGISTER(grompack_logger, LOG_LEVEL_DBG);

K_MSGQ_DEFINE(ble_data_queue, sizeof(struct neural_packet), 10, 4);

system_status_t status_flag = STATUS_OK;

int main(void) {
    int err;

    configure_timer();
    configure_saadc();
    configure_ppi();
    configure_ble();

    struct neural_packet tx_packet;

    while (1) {
        if (k_msgq_get(&ble_data_queue, &tx_packet, K_FOREVER) == 0) {
            if (is_laptop_subscribed) {
                err =
                    bt_nus_send(NULL, (uint8_t*)&tx_packet, sizeof(tx_packet));

                if (err && err != -EAGAIN && err != -ENOTCONN) {
                    // LOG_INF("Transmission error: %d\n", err);
                }
            } else {
                // LOG_INF("Laptop not subscribed, skipping transmission.\n");
            }
        }
    }
}