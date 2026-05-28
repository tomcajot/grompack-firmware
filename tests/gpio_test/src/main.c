#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

static const struct gpio_dt_spec toggle_pin =
    GPIO_DT_SPEC_GET(DT_NODELABEL(test_p0_02), gpios);

int main(void) {
    int ret;

    if (!gpio_is_ready_dt(&toggle_pin)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&toggle_pin, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }

    while (1) {
        ret = gpio_pin_toggle_dt(&toggle_pin);
        if (ret < 0) {
            return 0;
        }

        k_msleep(1000);
    }

    return 0;
}