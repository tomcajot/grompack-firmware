#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_REGISTER(grompack_logger, LOG_LEVEL_DBG);

// K_MSGQ_DEFINE(ble_data_queue, sizeof(struct neural_packet), 10, 4);

K_MEM_SLAB_DEFINE(ble_payload_slab, sizeof(struct neural_packet), 10, 4);

K_FIFO_DEFINE(ble_pointer_fifo);

K_MSGQ_DEFINE(command_queue, sizeof(uint8_t), 10, 4);

void command_thread_entry(void* param1, void* param2, void* param3) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(param3);

    uint8_t incoming_command;

    while (1) {
        if (k_msgq_get(&command_queue, &incoming_command, K_FOREVER) == 0) {
            LOG_INF("Processing command: 0x%02x", incoming_command);
            switch (incoming_command) {
                case 0x01:
                    LOG_INF("Command 0x01: Start Data Collection");
                    start_hardware_pipeline();
                    break;
                case 0x02:
                    LOG_INF("Command 0x02: Stop Data Collection");
                    stop_hardware_pipeline();
                    break;
                default:
                    LOG_WRN("Unknown command received: 0x%02x",
                            incoming_command);
            }
        }
    }
}

K_THREAD_DEFINE(command_thread_id, 1024, command_thread_entry, NULL, NULL, NULL,
                7, 0, 0);

int main(void) {
    int err;

    configure_timer();
    configure_saadc();
    configure_ppi();
    configure_ble();

    struct neural_packet* tx_packet;

    while (1) {
        tx_packet = k_fifo_get(&ble_pointer_fifo, K_FOREVER);

        if (is_laptop_subscribed) {
            err = bt_nus_send(NULL, (uint8_t*)&tx_packet->sample_index,
                              sizeof(uint32_t) + PACKED_BUFFER_SIZE);

            if (err) {
                if (err == -ENOMEM) {
                    LOG_WRN("Buffer full, dropping packet with index: %u",
                            tx_packet->sample_index);
                } else if (err != -EAGAIN && err != -ENOTCONN) {
                    LOG_ERR("Transmission error: %d", err);
                }
            } else {
                LOG_INF("Transmitting.");
            }
        } else {
            LOG_INF("Laptop not subscribed, skipping transmission.");
        }
        k_mem_slab_free(&ble_payload_slab, (void*)tx_packet);
    }
}