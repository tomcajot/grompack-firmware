#!/usr/bin/env python3

import time
import asyncio
import struct
import threading
from collections import deque
from bleak import BleakClient, BleakScanner

import matplotlib.pyplot as plt
import matplotlib.animation as animation

DEVICE_NAME       = "grompack"
NUS_TX_CHAR_UUID  = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NUS_SERVICE_UUID  = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID  = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
PACKED_BUFFER_SIZE = 240
PACKET_FORMAT      = f"<I{PACKED_BUFFER_SIZE}s"
PACKET_SIZE        = struct.calcsize(PACKET_FORMAT)


MAX_POINTS = 75

ch1_data = deque(maxlen=MAX_POINTS)
ch2_data = deque(maxlen=MAX_POINTS)

is_running = True
delays = []

def on_notify(_handle: int, data: bytearray) -> None:
    if len(data) < PACKET_SIZE:
        return

    _index, packed = struct.unpack_from(PACKET_FORMAT, data)

    samples = []
    for i in range(0, PACKED_BUFFER_SIZE, 3):
        b0, b1, b2 = packed[i], packed[i+1], packed[i+2]

        sample1 = b0 | ((b1 & 0x0F) << 8)
        sample2 = (b1 >> 4) | (b2 << 4)
        sample2 &= 0x0FFF

        samples.append((sample1, sample2))

    ch1 = [s1 for s1, s2 in samples]
    ch2 = [s2 for s1, s2 in samples]

    ch1_data.extend(ch1)
    ch2_data.extend(ch2)
    delays.append(time.time())

async def ble_task() -> None:
    global is_running
    print(f"Scanning for '{DEVICE_NAME}' …")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)

    if device is None:
        print("[ERROR] device not found")
        return

    print(f"Found {device.name} [{device.address}] — connecting …")

    async with BleakClient(device) as client:
        for svc in client.services:
            print(f"  Service: {svc.uuid}")
            for ch in svc.characteristics:
                print(f"    Char: {ch.uuid}  handle={ch.handle}  props={ch.properties}")

        nus_service = next(
            (s for s in client.services if s.uuid.lower() == NUS_SERVICE_UUID.lower()),
            None
        )
        if not nus_service:
            print(f"[ERROR] NUS service not found")
            return

        tx_char = next(
            (c for c in nus_service.characteristics
             if c.uuid.lower() == NUS_TX_CHAR_UUID.lower()),
            None
        )
        if not tx_char:
            print(f"[ERROR] TX characteristic not found")
            return

        rx_char = next(
            (c for c in nus_service.characteristics
             if c.uuid.lower() == NUS_RX_CHAR_UUID.lower()),
            None
        )
        if not rx_char:
            print(f"[ERROR] RX characteristic not found")
            return

        print(f"Using TX char at handle {tx_char.handle}")
        await client.start_notify(tx_char, on_notify)


        # await client.write_gatt_char(rx_char, b'\x01')
        await client.write_gatt_char(rx_char, b'\x03')
        
        while is_running:
            await asyncio.sleep(0.1)
            
        print("Stopping BLE notifications...")
        await client.stop_notify(tx_char)

def start_ble_background():
    """Wrapper to run the async BLE task in a separate thread."""
    asyncio.run(ble_task())

if __name__ == "__main__":
    ble_thread = threading.Thread(target=start_ble_background, daemon=True)
    ble_thread.start()

    fig, ax = plt.subplots()
    line1, = ax.plot([], [], lw=1.5, label="CH1")
    line2, = ax.plot([], [], lw=1.5, label="CH2", color='orange')

    ax.set_title(f"Live BLE Data ({DEVICE_NAME})")
    ax.set_ylabel("Raw ADC Value")
    ax.set_xlabel(f"Most recent {MAX_POINTS} samples")
    ax.set_xlim(0, MAX_POINTS)
    ax.legend()

    def update(frame):
        ch1_list = list(ch1_data)
        # ch2_list = list(ch2_data)

        if ch1_list:
            line1.set_data(range(len(ch1_list)), ch1_list)
        # if ch2_list:
            # line2.set_data(range(len(ch2_list)), ch2_list)

        ax.relim()
        ax.autoscale_view(scalex=False, scaley=True)

        return line1, line2

    ani = animation.FuncAnimation(fig, update, interval=50, cache_frame_data=False)

    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        print("\nPlot closed. Shutting down BLE thread...")
        is_running = False
        ble_thread.join()
        print("Disconnected.")