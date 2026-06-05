cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(grompack_firmware)

target_sources(app PRIVATE src/main.c src/timer.c src/saadc.c src/gppi.c src/ble.c src/stimulation.c src/commands.c src/config.c)