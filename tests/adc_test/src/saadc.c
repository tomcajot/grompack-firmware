#include <nrfx_saadc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(adc_logger, LOG_LEVEL_DBG);

static nrfx_saadc_channel_t channels[2] = {
    NRFX_SAADC_DEFAULT_CHANNEL_SE(SAADC_INPUT_PIN_0, 0),
    NRFX_SAADC_DEFAULT_CHANNEL_SE(SAADC_INPUT_PIN_1, 1)};

static int16_t saadc_sample_buffer[2][SAADC_BUFFER_SIZE];

// maybe we can use this as a packet index?
// if not then i would have wanted to turn this into a bool but thats
// not the best as later on they use %2 to count the index which is a
// "safer?" atomic instruction.
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
            int16_t sample = ((int16_t*)(p_event->data.done.p_buffer))[0];
            LOG_INF("Sample: %d", sample);
            break;
        default:
            LOG_INF("saadc_event_handler default state");
            break;
    }
}

void configure_saadc(void) {
    int err;

    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SAADC), IRQ_PRIO_LOWEST,
                nrfx_saadc_irq_handler, 0, 0);
    err = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
    if (err != 0) {
        LOG_ERR("nrfx_saadc_init error: %08x", err);
        return;
    }

    channels[0].channel_config.gain = NRF_SAADC_GAIN1_4;
    channels[1].channel_config.gain = NRF_SAADC_GAIN1_4;
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