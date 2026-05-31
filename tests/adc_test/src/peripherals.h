#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <nrfx_timer.h>

#define SAADC_SAMPLE_INTERVAL_US 100
#define SAADC_BUFFER_SIZE 80

#define SAADC_INPUT_PIN_0 NRFX_ANALOG_EXTERNAL_AIN4
#define SAADC_INPUT_PIN_1 NRFX_ANALOG_EXTERNAL_AIN5

#define SAADC_TIMER_INST_IDX 20

extern nrfx_timer_t timer_instance;

void configure_timer(void);
void configure_saadc(void);
void configure_ppi(void);

#endif