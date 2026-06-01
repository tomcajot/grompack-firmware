#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_REGISTER(adc_logger, LOG_LEVEL_DBG);

int main(void) {
    configure_timer();
    configure_saadc();
    configure_ppi();
    k_sleep(K_FOREVER);
}