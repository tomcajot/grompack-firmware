#include <nrfx_saadc.h>
#include <nrfx_timer.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

nrfx_timer_t timer_instance =
    NRFX_TIMER_INSTANCE(NRF_TIMER_INST_GET(SAADC_TIMER_INST_IDX));

void configure_timer(void) {
    int err;

    uint32_t base_frequency =
        NRF_TIMER_BASE_FREQUENCY_GET(timer_instance.p_reg);
    nrfx_timer_config_t timer_config =
        NRFX_TIMER_DEFAULT_CONFIG(base_frequency);

    err = nrfx_timer_init(&timer_instance, &timer_config, NULL);
    if (err != 0) {
        LOG_ERR("nrfx_timer_init error: %08x", err);
        return;
    }
    uint32_t timer_ticks =
        nrfx_timer_us_to_ticks(&timer_instance, SAADC_SAMPLE_INTERVAL_US);
    nrfx_timer_extended_compare(&timer_instance, NRF_TIMER_CC_CHANNEL0,
                                timer_ticks,
                                NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, false);
}