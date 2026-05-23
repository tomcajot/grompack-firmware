#include <helpers/nrfx_gppi.h>
#include <nrfx_saadc.h>
#include <nrfx_timer.h>
#include <zephyr/kernel.h>

#include "peripherals.h"

void configure_ppi(void) {
    int err;

    nrfx_gppi_handle_t gppi_handle_sample;
    nrfx_gppi_handle_t gppi_handle_start;

    err = nrfx_gppi_conn_alloc(
        nrfx_timer_compare_event_address_get(&timer_instance,
                                             NRF_TIMER_CC_CHANNEL0),
        nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE),
        &gppi_handle_sample);
    if (err != 0) {
        status_flag = ERROR;
        return;
    }

    err = nrfx_gppi_conn_alloc(
        nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_END),
        nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_START),
        &gppi_handle_start);
    if (err != 0) {
        status_flag = ERROR;
        return;
    }

    nrfx_gppi_conn_enable(gppi_handle_sample);
    nrfx_gppi_conn_enable(gppi_handle_start);
}
