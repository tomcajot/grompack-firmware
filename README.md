# GromPack Firmware

Firmware for **GromPack** — a wireless neurorecording and neurostimulation device for the Madagascar Hissing Cockroach (MHC).

🌐 Website: [grompack.nl](https://grompack.nl)

## Overview

GromPack Firmware runs on the GromPack MCU and handles wireless data acquisition and stimulation control for the device. It is designed to reliably combine low-latency BLE communication with real-time analog signal capture.

## MCU Firmware Structure

The firmware is built around three core responsibilities:

- **BLE communication** — the device advertises continuously while powered on and establishes a connection whenever possible.
- **ADC reading & streaming** — analog data is read and streamed over BLE, triggered by a specific incoming command.
- **Control system** — commands can be sent and received simultaneously, enabling real-time control alongside data streaming.

## Repository Structure

```
.
├── app/                                         # Main firmware application
├── boards/Team_Kakkerlak/gromboard-minewsemi/   # Board-specific configuration
└── tests/                                       # Test suite
```

## Getting Started

Clone the repository:

```bash
git clone https://github.com/tomcajot/grompack-firmware.git
cd grompack-firmware
```

> Add build/flash instructions here once your toolchain setup is finalized (e.g. West/Zephyr build commands for the `gromboard-minewsemi` board).

## Tech Stack

- **C** — core firmware logic
- **CMake** — build system
- **C++** — supporting components

## About the Project

GromPack is developed to enable wireless neural recording and stimulation research on the Madagascar Hissing Cockroach. Learn more about the project at [grompack.nl](https://grompack.nl).

## License

_Add license information here._
