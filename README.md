# GromPack Firmware

This repository contains the firmware for the GromPack device.

## MCU Firmware Structure

- Receiving BLE commands (device should constantly advertise when ON and establish connection when possible)
- Reading ADC & sending over BLE (should happen when BLE receives specific command)
- Control system (should be able to send and receive commands at the same time)
