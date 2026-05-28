#include <nrfx_saadc.h>
#include <zephyr/kernel.h>

#include "peripherals.h"

// LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

static struct neural_packet current_tx_packet;
static uint16_t byte_fill_count = 0;
static uint32_t global_sample_counter = 0;

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
                status_flag = ERROR;
                return;
            }
            break;

        case NRFX_SAADC_EVT_DONE: {
            int16_t* raw_data = (int16_t*)(p_event->data.done.p_buffer);

            // LOG_INF("Sample: %d", (int)raw_data[0]);

            for (int i = 0; i < (SAADC_BUFFER_SIZE / 2); i++) {
                uint16_t sample1 = (uint16_t)raw_data[i * 2] & 0x0FFF;
                uint16_t sample2 = (uint16_t)raw_data[(i * 2) + 1] & 0x0FFF;

                current_tx_packet.packed_data[byte_fill_count++] =
                    sample1 & 0xFF;
                current_tx_packet.packed_data[byte_fill_count++] =
                    ((sample1 >> 8) & 0x0F) | ((sample2 << 4) & 0xF0);
                current_tx_packet.packed_data[byte_fill_count++] =
                    (sample2 >> 4) & 0xFF;

                if (byte_fill_count >= PACKED_BUFFER_SIZE) {
                    current_tx_packet.sample_index = global_sample_counter;
                    err = k_msgq_put(&ble_data_queue, &current_tx_packet,
                                     K_NO_WAIT);
                    if (err != 0) {
                        status_flag = ERROR;
                        return;
                    }
                    byte_fill_count = 0;
                    global_sample_counter += (SAADC_BUFFER_SIZE / 2);
                }
            }
            break;
        } /* NRFX_SAADC_EVT_DONE */
        default:
            status_flag = ERROR;
            break;
    }
}

void configure_saadc(void) {
    int err;

    IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SAADC), IRQ_PRIO_LOWEST,
                nrfx_saadc_irq_handler, 0, 0);
    err = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
    if (err != 0) {
        status_flag = ERROR;
        return;
    }

    channels[0].channel_config.gain = NRF_SAADC_GAIN1_4;
    channels[1].channel_config.gain = NRF_SAADC_GAIN1_4;
    err = nrfx_saadc_channels_config(channels, 2);
    if (err != 0) {
        status_flag = ERROR;
        return;
    }

    nrfx_saadc_adv_config_t saadc_adv_config = NRFX_SAADC_DEFAULT_ADV_CONFIG;
    err = nrfx_saadc_advanced_mode_set(BIT(0) | BIT(1),
                                       NRF_SAADC_RESOLUTION_12BIT,
                                       &saadc_adv_config, saadc_event_handler);
    if (err != 0) {
        status_flag = ERROR;
        return;
    }

    err = nrfx_saadc_buffer_set(saadc_sample_buffer[0], SAADC_BUFFER_SIZE);
    if (err != 0) {
        status_flag = ERROR;
        return;
    }

    err = nrfx_saadc_buffer_set(saadc_sample_buffer[1], SAADC_BUFFER_SIZE);
    if (err != 0) {
        status_flag = ERROR;
        return;
    }

    err = nrfx_saadc_mode_trigger();
    if (err != 0) {
        status_flag = ERROR;
        return;
    }
}