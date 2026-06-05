#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "peripherals.h"

LOG_MODULE_DECLARE(grompack_logger, LOG_LEVEL_DBG);

void command_thread_entry(void* param1, void* param2, void* param3) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(param3);

    uint8_t incoming_command;

    while (1) {
        if (k_msgq_get(&command_queue, &incoming_command, K_FOREVER) == 0) {
            LOG_INF("Processing command: 0x%02x", incoming_command);
            switch (incoming_command) {
                case 0x01:
                    LOG_INF("start data collection");
                    start_hardware_pipeline();
                    break;
                case 0x02:
                    LOG_INF("stop data collection");
                    stop_hardware_pipeline();
                    break;
                case 0x03:
                    LOG_INF("turn on pwm continuous");
                    set_stimulation_continuous(true);
                    break;
                case 0x04:
                    LOG_INF("turn off pwm continous");
                    set_stimulation_continuous(false);
                    break;
                case 0x05:
                    LOG_INF("set pwm burst");
                    uint32_t time = 3;
                    uint32_t frequency = 1000;
                    set_stimulation_burst(time, frequency);
                    break;
                case 0x06:
                    LOG_INF("power off");
                    system_off();
                    break;
                default:
                    LOG_WRN("Unknown command received: 0x%02x",
                            incoming_command);
            }
        }
    }
}