#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <nrfx_timer.h>

#define SAADC_SAMPLE_INTERVAL_US 1000
#define SAADC_BUFFER_SIZE 8

#define SAADC_INPUT_PIN_0 NRFX_ANALOG_EXTERNAL_AIN4
#define SAADC_INPUT_PIN_1 NRFX_ANALOG_EXTERNAL_AIN5

#define SAADC_TIMER_INST_IDX 20

extern nrfx_timer_t timer_instance;

typedef enum {
    STATUS_OK = 0,
    ERROR = BIT(1),
    // dees kunnen we extenden om wat meer error types te hebben en dan kunnen
    // we elke keer |= om die bij te houden. da werkt met Segger RTT dan kunnen
    // we op de SWD ook soort van debuggen.
} system_status_t;

extern system_status_t status_flag;

void configure_timer(void);
void configure_saadc(void);
void configure_ppi(void);

#endif