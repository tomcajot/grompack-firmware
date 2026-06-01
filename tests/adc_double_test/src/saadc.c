#include <nrfx_saadc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(adc_logger, LOG_LEVEL_DBG);

static nrfx_saadc_channel_t channels[2] = {
    NRFX_SAADC_DEFAULT_CHANNEL_SE(NRFX_ANALOG_EXTERNAL_AIN4, 0),
    NRFX_SAADC_DEFAULT_CHANNEL_SE(NRFX_ANALOG_EXTERNAL_AIN5, 1)};

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

            int16_t* raw_data = (int16_t*)(p_event->data.done.p_buffer);

            for (int i = 0; i < (SAADC_BUFFER_SIZE / 2); i++) {
                int16_t raw1 = raw_data[i * 2];
                int16_t raw2 = raw_data[(i * 2) + 1];

                if (raw1 < 0) raw1 = 0;
                if (raw2 < 0) raw2 = 0;

                uint16_t sample1 = (uint16_t)raw1 & 0x0FFF;
                uint16_t sample2 = (uint16_t)raw2 & 0x0FFF;

                LOG_INF("Sample 1: %d; Sample 2: %d", sample1,
                        sample2);  // this is a bad way of doing it, we call a
                                   // function from an interrupt happening very
                                   // fast, we send a lot of cmnds to the
                                   // logging tool so it cant keep up and you
                                   // dont see the actual rate logged. nice for
                                   // simple debugging though.
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

    channels[0].channel_config.gain = NRF_SAADC_GAIN1_4;
    channels[0].channel_config.acq_time = NRF_SAADC_ACQTIME_MAX;
    channels[1].channel_config.gain = NRF_SAADC_GAIN1_4;
    channels[1].channel_config.acq_time = NRF_SAADC_ACQTIME_MAX;

    err = nrfx_saadc_channels_config(channels, 2);
    if (err != 0) {
        LOG_ERR("nrfx_saadc_channels_config error: %08x", err);
        return;
    }

    nrfx_saadc_adv_config_t saadc_adv_config = NRFX_SAADC_DEFAULT_ADV_CONFIG;
    err = nrfx_saadc_advanced_mode_set(BIT(0) | BIT(1),
                                       NRF_SAADC_RESOLUTION_12BIT,
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
