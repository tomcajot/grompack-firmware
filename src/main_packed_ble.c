#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/nus.h>

#include "peripherals.h" 
#include "ble.h"

K_MSGQ_DEFINE(ble_data_queue, sizeof(struct neural_packet), 10, 4);

int main(void) {
    
    int err;

    configure_timer();
    configure_saadc();
    configure_ppi();

    initialize_bluetooth();
    
    struct neural_packet tx_packet;
    
    while (1) {

        if (k_msgq_get(&ble_data_queue, &tx_packet, K_FOREVER) == 0) {
            
            if (is_laptop_subscribed) {

                err = bt_nus_send(NULL, (uint8_t *)&tx_packet, sizeof(tx_packet));

                if (err && err != -EAGAIN && err != -ENOTCONN) {
                printk("Transmission error: %d\n", err);
            }
            } else {
                printk("Laptop not subscribed, skipping transmission.\n");
            }
        }
    }
}