
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

const struct device* gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
const struct device* gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
const struct device* gpio2 = DEVICE_DT_GET(DT_NODELABEL(gpio2));

void setup_unused_pins(void) {
    if (!device_is_ready(gpio0) || !device_is_ready(gpio1) ||
        !device_is_ready(gpio2)) {
        return;
    }

    gpio_pin_configure(gpio0, 0, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio0, 1, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio0, 2, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio0, 3, GPIO_DISCONNECTED);

    gpio_pin_configure(gpio1, 2, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio1, 3, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio1, 4, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio1, 9, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio1, 10, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio1, 11, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio1, 12, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio1, 13, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio1, 14, GPIO_DISCONNECTED);

    gpio_pin_configure(gpio2, 3, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio2, 6, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio2, 7, GPIO_DISCONNECTED);
    gpio_pin_configure(gpio2, 10, GPIO_DISCONNECTED);
}

void system_off(void) {
    LOG_INF("Power off ...");
    sys_poweroff();
}