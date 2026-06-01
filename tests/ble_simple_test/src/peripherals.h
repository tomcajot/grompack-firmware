#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <stdbool.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>

#define PACKED_BUFFER_SIZE 240

struct neural_packet {
    uint32_t sample_index;
    uint8_t packed_data[PACKED_BUFFER_SIZE];
} __packed;

extern struct k_msgq ble_data_queue;
extern struct k_timer fake_timer;
extern bool is_laptop_subscribed;

void configure_ble(void);

#endif