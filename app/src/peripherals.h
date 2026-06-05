#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <nrfx_timer.h>
#include <stdbool.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>

#define PACKED_BUFFER_SIZE 240

#define SAADC_SAMPLE_INTERVAL_US 80
#define SAADC_BUFFER_SIZE 16
#define SAADC_INPUT_PIN_0 NRFX_ANALOG_EXTERNAL_AIN4
#define SAADC_INPUT_PIN_1 NRFX_ANALOG_EXTERNAL_AIN5
#define SAADC_TIMER_INST_IDX 20
#define PWM_INST_IDX 20
#define NRF54_SAADC_ACQTIME_US(us) (((us * 1000) / 125) - 1)

#define STIM_LEFT_NODE DT_NODELABEL(stim_left)
#define STIM_LEFT_PORT DT_PROP(DT_GPIO_CTLR(STIM_LEFT_NODE, gpios), port)
#define STIM_LEFT_PIN DT_GPIO_PIN(STIM_LEFT_NODE, gpios)

#define STIM_LEFT_ABS_PIN ((STIM_LEFT_PORT * 32) + STIM_LEFT_PIN)

#define STIM_RIGHT_NODE DT_NODELABEL(stim_right)
#define STIM_RIGHT_PORT DT_PROP(DT_GPIO_CTLR(STIM_RIGHT_NODE, gpios), port)
#define STIM_RIGHT_PIN DT_GPIO_PIN(STIM_RIGHT_NODE, gpios)

#define STIM_RIGHT_ABS_PIN ((STIM_RIGHT_PORT * 32) + STIM_RIGHT_PIN)

extern nrfx_timer_t timer_instance;

struct neural_packet {
    void* fifo_reserved;
    uint32_t sample_index;
    uint8_t packed_data[PACKED_BUFFER_SIZE];
} __packed;

extern struct k_fifo ble_pointer_fifo;
extern struct k_mem_slab ble_payload_slab;
extern struct k_msgq command_queue;

extern volatile bool is_laptop_subscribed;

void setup_unused_pins(void);
void system_off(void);
void configure_stimulation(void);
void configure_ble(void);
void command_thread_entry(void* param1, void* param2, void* param3);
void configure_timer(void);
void configure_saadc(void);
void configure_ppi(void);
void start_hardware_pipeline(void);
void stop_hardware_pipeline(void);
void purge_pipeline(void);
void set_stimulation_continuous(bool start_pwm);
void set_stimulation_burst(uint32_t duration_ms, uint32_t frequency_hz);

#endif