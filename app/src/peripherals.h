#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <nrfx_timer.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>

#define PACKED_BUFFER_SIZE 240

// todo change to 80 and check in saadc if acq max still works or adapt.
#define SAADC_SAMPLE_INTERVAL_US 100
#define SAADC_BUFFER_SIZE 16
#define SAADC_INPUT_PIN_0 NRFX_ANALOG_EXTERNAL_AIN4
#define SAADC_INPUT_PIN_1 NRFX_ANALOG_EXTERNAL_AIN5
#define SAADC_TIMER_INST_IDX 20

extern nrfx_timer_t timer_instance;

struct neural_packet {
    void *fifo_reserved;
    uint32_t sample_index;
    uint8_t packed_data[PACKED_BUFFER_SIZE];
} __packed;

//extern struct k_msgq ble_data_queue;
extern struct k_fifo ble_pointer_fifo;
extern struct k_mem_slab ble_payload_slab;

#include <stdbool.h>

extern bool is_laptop_subscribed;

void configure_ble(void);
void configure_timer(void);
void configure_saadc(void);
void configure_ppi(void);
void start_hardware_pipeline(void);
void stop_hardware_pipeline(void);
void purge_pipeline(void);

#endif