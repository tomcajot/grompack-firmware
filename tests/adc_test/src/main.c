#include <zephyr/kernel.h>

#include "peripherals.h"

int main(void) {
    configure_timer();
    configure_saadc();
    configure_ppi();
    k_sleep(K_FOREVER);
}