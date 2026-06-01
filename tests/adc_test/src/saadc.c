#include <nrfx_saadc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(adc_logger, LOG_LEVEL_DBG);

static nrfx_saadc_channel_t channel =
    NRFX_SAADC_DEFAULT_CHANNEL_SE(NRFX_ANALOG_EXTERNAL_AIN5, 0);

static int16_t saadc_sample_buffer[2][SAADC_BUFFER_SIZE];

static uint32_t saadc_current_buffer = 0;

static void saadc_event_handler(nrfx_saadc_evt_t const* p_event) {
    int err;
    switch (p_event->type) {
        case NRFX_SAADC_EVT_READY:
            nrfx_timer_enable(&timer_instance);
            break;

        case NRFX_SAADC_EVT_BUF_REQ:
            err = nrfx_saadc_buffer_set(
                saadc_sample_buffer[(saadc_current_buffer++) % 2],
                SAADC_BUFFER_SIZE);
            if (err != 0) {
                LOG_ERR("nrfx_saadc_buffer_set error: %08x", err);
                return;
            }
            break;

        case NRFX_SAADC_EVT_DONE:
            int16_t current_value;
            for (int i = 0; i < p_event->data.done.size; i++) {
                current_value = ((int16_t*)(p_event->data.done.p_buffer))[i];
                LOG_INF("SAADC %d", current_value);
            }

            break;
        default:
            LOG_INF("Unhandled SAADC evt %d", p_event->type);
            break;
    }
}

void configure_saadc(void) {
    int err;

    IRQ_CONNECT(DT_IRQN(DT_NODELABEL(adc)), DT_IRQ(DT_NODELABEL(adc), priority),
                nrfx_isr, nrfx_saadc_irq_handler, 0);

    err = nrfx_saadc_init(DT_IRQ(DT_NODELABEL(adc), priority));
    if (err != 0) {
        LOG_ERR("nrfx_saadc_init error: %08x", err);
        return;
    }

    channel.channel_config.gain = NRF_SAADC_GAIN1_4;
    channel.channel_config.acq_time = NRF_SAADC_ACQTIME_MAX;

    err = nrfx_saadc_channels_config(&channel, 1);
    if (err != 0) {
        LOG_ERR("nrfx_saadc_channels_config error: %08x", err);
        return;
    }

    nrfx_saadc_adv_config_t saadc_adv_config = NRFX_SAADC_DEFAULT_ADV_CONFIG;
    err = nrfx_saadc_advanced_mode_set(BIT(0), NRF_SAADC_RESOLUTION_12BIT,
                                       &saadc_adv_config, saadc_event_handler);
    if (err != 0) {
        LOG_ERR("nrfx_saadc_advanced_mode_set error: %08x", err);
        return;
    }

    err = nrfx_saadc_buffer_set(saadc_sample_buffer[0], SAADC_BUFFER_SIZE);
    if (err != 0) {
        LOG_ERR("nrfx_saadc_buffer_set error: %08x", err);
        return;
    }
    err = nrfx_saadc_buffer_set(saadc_sample_buffer[1], SAADC_BUFFER_SIZE);
    if (err != 0) {
        LOG_ERR("nrfx_saadc_buffer_set error: %08x", err);
        return;
    }

    err = nrfx_saadc_mode_trigger();
    if (err != 0) {
        LOG_ERR("nrfx_saadc_mode_trigger error: %08x", err);
        return;
    }
}
