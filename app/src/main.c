#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_REGISTER(grompack_logger, LOG_LEVEL_DBG);
K_MEM_SLAB_DEFINE(ble_payload_slab, sizeof(struct neural_packet), 240, 4);
K_FIFO_DEFINE(ble_pointer_fifo);
K_MSGQ_DEFINE(command_queue, sizeof(uint8_t), 10, 4);
K_THREAD_DEFINE(command_thread_id, 1024, command_thread_entry, NULL, NULL, NULL,
                7, 0, 0);

int main(void) {
    int err;

    setup_unused_pins();
    configure_stimulation();
    configure_timer();
    configure_saadc();
    configure_ppi();
    configure_ble();

    struct neural_packet* tx_packet;

    while (1) {
        tx_packet = k_fifo_get(&ble_pointer_fifo, K_FOREVER);

        if (is_laptop_subscribed) {
            bool packet_sent = false;

            while (!packet_sent && is_laptop_subscribed) {
                err = bt_nus_send(NULL, (uint8_t*)&tx_packet->sample_index,
                                  sizeof(uint32_t) + PACKED_BUFFER_SIZE);

                if (err == 0) {
                    packet_sent = true;
                } else if (err == -ENOMEM) {
                    k_yield();
                } else if (err != -EAGAIN) {
                    LOG_ERR("Transmission fatal error: %d", err);
                    break;
                }
            }
        } else {
            LOG_INF("Laptop not subscribed, skipping transmission.");
        }

        k_mem_slab_free(&ble_payload_slab, (void*)tx_packet);
    }
}