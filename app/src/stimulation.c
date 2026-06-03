#include <nrfx_pwm.h>
#include <stdbool.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

static const struct gpio_dt_spec rec_stim_sw =
    GPIO_DT_SPEC_GET(DT_NODELABEL(rec_stim_sw), gpios);

static const struct gpio_dt_spec pwm_test_pin =
    GPIO_DT_SPEC_GET(DT_NODELABEL(pwm_test_pin), gpios);

static nrfx_pwm_t pwm_instance =
    NRFX_PWM_INSTANCE(NRF_PWM_INST_GET(PWM_INST_IDX));

static nrf_pwm_values_common_t seq_values[] = {500};

static nrf_pwm_sequence_t seq = {.values.p_common = seq_values,
                                 .length = NRFX_ARRAY_SIZE(seq_values),
                                 .repeats = 0,
                                 .end_delay = 0};

void set_stimulation_continuous(bool start_pwm) {
    if (start_pwm) {
        nrfx_pwm_simple_playback(&pwm_instance, &seq, 1, NRFX_PWM_FLAG_LOOP);
        LOG_INF("Stimulation PWM Started");
    } else {
        nrfx_pwm_stop(&pwm_instance, false);
        LOG_INF("Stimulation PWM Stopped");
    }
}

void set_stimulation_burst(uint32_t duration_ms, uint32_t frequency_hz) {
    uint32_t total_cycles = (frequency_hz * duration_ms) / 1000;

    LOG_INF("Starting PWM Burst: %u ms at %u Hz (%u cycles)", duration_ms,
            frequency_hz, total_cycles);

    nrfx_pwm_simple_playback(&pwm_instance, &seq, total_cycles,
                             NRFX_PWM_FLAG_STOP);
}

void configure_stimulation(void) {
    int err;

    if (!gpio_is_ready_dt(&rec_stim_sw)) {
        LOG_INF("REC_STIM_SW GPIO not ready");
    }

    if (!gpio_is_ready_dt(&pwm_test_pin)) {
        LOG_INF("PWM_TEST_PIN GPIO not ready");
    }

    err = gpio_pin_configure_dt(&rec_stim_sw, GPIO_OUTPUT_LOW);
    if (err < 0) {
        LOG_ERR("REC_STIM_SW GPIO config error (%d)", err);
    }

    uint32_t nrf_pin_number = 32 + pwm_test_pin.pin;

    nrfx_pwm_config_t config = NRFX_PWM_DEFAULT_CONFIG(
        nrf_pin_number, NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED,
        NRF_PWM_PIN_NOT_CONNECTED);

    config.base_clock = NRF_PWM_CLK_1MHz;
    config.top_value = 1000;
    config.load_mode = NRF_PWM_LOAD_COMMON;

    err = nrfx_pwm_init(&pwm_instance, &config, NULL, NULL);
    LOG_INF("PWM init with response: %08x", err);
}