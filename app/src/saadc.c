#include <nrfx_saadc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

static struct neural_packet* current_tx_packet = NULL;
static uint16_t byte_fill_count = 0;
static uint32_t global_sample_counter = 0;

static nrfx_saadc_channel_t channels[2] = {
    NRFX_SAADC_DEFAULT_CHANNEL_DIFFERENTIAL(SAADC_INPUT_PIN_0,
                                            SAADC_INPUT_PIN_REF, 0),
    NRFX_SAADC_DEFAULT_CHANNEL_DIFFERENTIAL(SAADC_INPUT_PIN_1,
                                            SAADC_INPUT_PIN_REF, 1)};

static int16_t saadc_sample_buffer[2][SAADC_BUFFER_SIZE];

static uint32_t saadc_current_buffer = 0;

void purge_pipeline(void) {
    if (current_tx_packet != NULL) {
        k_mem_slab_free(&ble_payload_slab, (void*)current_tx_packet);
        current_tx_packet = NULL;
    }

    byte_fill_count = 0;

    struct neural_packet* stale_packet;
    while ((stale_packet = k_fifo_get(&ble_pointer_fifo, K_NO_WAIT)) != NULL) {
        k_mem_slab_free(&ble_payload_slab, (void*)stale_packet);
    }

    LOG_INF("Pipeline flushed and memory returned to slab.");
}

void stop_hardware_pipeline(void) {
    LOG_INF("Halting hardware pipeline...");

    nrfx_timer_disable(&timer_instance);
    nrfx_timer_clear(&timer_instance);

    k_busy_wait(500);
    nrfx_saadc_abort();
    k_busy_wait(200);

    purge_pipeline();
}

void start_hardware_pipeline(void) {
    LOG_INF("Booting hardware pipeline...");

    global_sample_counter = 0;
    saadc_current_buffer = 0;

    nrfx_saadc_buffer_set(saadc_sample_buffer[0], SAADC_BUFFER_SIZE);
    nrfx_saadc_buffer_set(saadc_sample_buffer[1], SAADC_BUFFER_SIZE);

    nrfx_saadc_mode_trigger();

    nrfx_timer_enable(&timer_instance);
}

static void saadc_event_handler(nrfx_saadc_evt_t const* p_event) {
    int err;
    switch (p_event->type) {
        case NRFX_SAADC_EVT_READY:
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

        case NRFX_SAADC_EVT_DONE: {
            if (current_tx_packet == NULL) {
                if (k_mem_slab_alloc(&ble_payload_slab,
                                     (void**)&current_tx_packet,
                                     K_NO_WAIT) != 0) {
                    global_sample_counter += (SAADC_BUFFER_SIZE / 2);
                    break;
                }
            }

            current_tx_packet->sample_index = global_sample_counter;

            int16_t* raw_data = (int16_t*)(p_event->data.done.p_buffer);

            for (int i = 0; i < (SAADC_BUFFER_SIZE / 2); i += 2) {
                uint16_t s0 = (uint16_t)raw_data[i * 2] & 0x3FF;
                uint16_t s1 = (uint16_t)raw_data[i * 2 + 1] & 0x3FF;
                uint16_t s2 = (uint16_t)raw_data[(i + 1) * 2] & 0x3FF;
                uint16_t s3 = (uint16_t)raw_data[(i + 1) * 2 + 1] & 0x3FF;

                current_tx_packet->packed_data[byte_fill_count++] = s0 & 0xFF;
                current_tx_packet->packed_data[byte_fill_count++] =
                    ((s0 >> 8) & 0x03) | ((s1 << 2) & 0xFC);
                current_tx_packet->packed_data[byte_fill_count++] =
                    ((s1 >> 6) & 0x0F) | ((s2 << 4) & 0xF0);
                current_tx_packet->packed_data[byte_fill_count++] =
                    ((s2 >> 4) & 0x3F) | ((s3 << 6) & 0xC0);
                current_tx_packet->packed_data[byte_fill_count++] =
                    (s3 >> 2) & 0xFF;

                global_sample_counter += 2;

                if (byte_fill_count >= PACKED_BUFFER_SIZE) {
                    k_fifo_put(&ble_pointer_fifo, current_tx_packet);
                    current_tx_packet = NULL;
                    byte_fill_count = 0;
                }
            }
            break;
        }
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

    channels[0].channel_config.gain = NRF_SAADC_GAIN1_2;
    channels[0].channel_config.acq_time = NRF54_SAADC_ACQTIME_US(20);
    channels[1].channel_config.gain = NRF_SAADC_GAIN1_2;
    channels[1].channel_config.acq_time = NRF54_SAADC_ACQTIME_US(20);

    err = nrfx_saadc_channels_config(channels, 2);
    if (err != 0) {
        LOG_ERR("nrfx_saadc_channels_config error: %08x", err);
        return;
    }

    nrfx_saadc_adv_config_t saadc_adv_config = NRFX_SAADC_DEFAULT_ADV_CONFIG;
    saadc_adv_config.oversampling = NRF_SAADC_OVERSAMPLE_4X;
    saadc_adv_config.burst = NRF_SAADC_BURST_ENABLED;

    err = nrfx_saadc_advanced_mode_set(BIT(0) | BIT(1),
                                       NRF_SAADC_RESOLUTION_10BIT,
                                       &saadc_adv_config, saadc_event_handler);

    if (err != 0) {
        LOG_ERR("nrfx_saadc_advanced_mode_set error: %08x", err);
        return;
    }
}