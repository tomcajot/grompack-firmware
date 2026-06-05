#include <nrfx_pwm.h>
#include <stdbool.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

static const struct gpio_dt_spec rec_stim_sw =
    GPIO_DT_SPEC_GET(DT_NODELABEL(rec_stim_sw), gpios);

static const struct gpio_dt_spec stim_right =
    GPIO_DT_SPEC_GET(DT_NODELABEL(stim_right), gpios);

static const struct gpio_dt_spec stim_left =
    GPIO_DT_SPEC_GET(DT_NODELABEL(stim_left), gpios);

static nrfx_pwm_t pwm_instance =
    NRFX_PWM_INSTANCE(NRF_PWM_INST_GET(PWM_INST_IDX));

static nrf_pwm_values_common_t seq_values[] = {500};

static nrf_pwm_sequence_t seq = {.values.p_common = seq_values,
                                 .length = NRFX_ARRAY_SIZE(seq_values),
                                 .repeats = 0,
                                 .end_delay = 0};

static void pwm_event_handler(nrfx_pwm_event_type_t event_type,
                              void* p_context) {
    if (event_type == NRFX_PWM_EVENT_STOPPED) {
        gpio_pin_set_dt(&rec_stim_sw, 0);
        LOG_INF("PWM Burst complete, switch pulled low.");
    }
}

void set_stimulation_continuous(bool start_pwm) {
    if (start_pwm) {
        gpio_pin_configure_dt(&rec_stim_sw, GPIO_OUTPUT_HIGH);
        nrfx_pwm_simple_playback(&pwm_instance, &seq, 1, NRFX_PWM_FLAG_LOOP);
        LOG_INF("Stimulation PWM Started");
    } else {
        nrfx_pwm_stop(&pwm_instance, false);
        gpio_pin_configure_dt(&rec_stim_sw, GPIO_OUTPUT_LOW);
        LOG_INF("Stimulation PWM Stopped");
    }
}

void set_stimulation_burst(uint32_t duration_ms, uint32_t frequency_hz) {
    uint16_t top_value = 1000000 / frequency_hz;
    seq_values[0] = top_value / 2;

    nrf_pwm_configure(pwm_instance.p_reg, NRF_PWM_CLK_1MHz, NRF_PWM_MODE_UP,
                      top_value);

    uint32_t total_cycles = (frequency_hz * duration_ms) / 1000;

    LOG_INF("Starting PWM Burst: %u ms at %u Hz (%u cycles)", duration_ms,
            frequency_hz, total_cycles);

    gpio_pin_set_dt(&rec_stim_sw, 1);

    nrfx_pwm_simple_playback(&pwm_instance, &seq, total_cycles,
                             NRFX_PWM_FLAG_STOP);
}

void configure_stimulation(void) {
    int err;

    if (!gpio_is_ready_dt(&rec_stim_sw) || !gpio_is_ready_dt(&stim_right) ||
        !gpio_is_ready_dt(&stim_left)) {
        LOG_ERR("One or more stimulation GPIOs not ready");
        return;
    }

    err = gpio_pin_configure_dt(&rec_stim_sw, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("REC_STIM_SW GPIO config error (%d)", err);
    }

    nrfx_pwm_config_t config = NRFX_PWM_DEFAULT_CONFIG(
        STIM_LEFT_ABS_PIN, STIM_RIGHT_ABS_PIN, NRF_PWM_PIN_NOT_CONNECTED,
        NRF_PWM_PIN_NOT_CONNECTED);

    config.base_clock = NRF_PWM_CLK_1MHz;
    config.top_value = 1000;
    config.load_mode = NRF_PWM_LOAD_COMMON;

    err = nrfx_pwm_init(&pwm_instance, &config, pwm_event_handler, NULL);
    if (err) {
        LOG_INF("error: %08x", err);
    }
}