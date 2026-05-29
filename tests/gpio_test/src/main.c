#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gpio_logger, LOG_LEVEL_DBG);

static const struct gpio_dt_spec toggle_pin =
    GPIO_DT_SPEC_GET(DT_NODELABEL(test_p0_00), gpios);

int main(void) {
    int ret;

    if (!gpio_is_ready_dt(&toggle_pin)) {
        LOG_INF("GPIO not ready");
        return 0;
    }

    ret = gpio_pin_configure_dt(&toggle_pin, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("GPIO config error (%d)", ret);
        return 0;
    }

    while (1) {
        ret = gpio_pin_toggle_dt(&toggle_pin);
        if (ret < 0) {
            LOG_ERR("GPIO toggle error (%d)", ret);
            return 0;
        }

        k_msleep(1000);
    }

    return 0;
}