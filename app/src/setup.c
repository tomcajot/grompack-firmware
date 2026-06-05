// #include <zephyr/drivers/gpio.h>
// #include <zephyr/kernel.h>
// #include <zephyr/logging/log.h>

// #include "peripherals.h"

// LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

// static const struct gpio_dt_spec p0_00 =
//     GPIO_DT_SPEC_GET(DT_NODELABEL(p0_00), gpios);

// void setup(void) {
//     int err;

//     const struct device* p0_00 = DEVICE_DT_GET(DT_NODELABEL(p0_00));
//     const struct device* p0_00 = DEVICE_DT_GET(DT_NODELABEL(p0_00));
//     const struct device* p0_00 = DEVICE_DT_GET(DT_NODELABEL(p0_00));
//     const struct device* p0_00 = DEVICE_DT_GET(DT_NODELABEL(p0_00));
//     const struct device* p0_00 = DEVICE_DT_GET(DT_NODELABEL(p0_00));

//     if (!gpio_is_ready_dt(&rec_stim_sw)) {
//         LOG_INF("REC_STIM_SW GPIO not ready");
//     }
// }