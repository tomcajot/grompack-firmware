import asyncio
import struct
import threading
from collections import deque

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from bleak import BleakClient, BleakScanner

DEVICE_NAME       = "grompack"
NUS_TX_CHAR_UUID  = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
PACKED_BUFFER_SIZE = 240
PACKET_FORMAT      = f"<I{PACKED_BUFFER_SIZE}s"
PACKET_SIZE        = struct.calcsize(PACKET_FORMAT)
MAX_POINTS         = 500

ch0 = deque(maxlen=MAX_POINTS)
ch1 = deque(maxlen=MAX_POINTS)

def on_notify(_handle: int, data: bytearray) -> None:
    if len(data) < PACKET_SIZE:
        return

    _index, packed = struct.unpack_from(PACKET_FORMAT, data)

    for i in range(0, PACKED_BUFFER_SIZE, 3):           # every 3 bytes = two 12-bit samples
        b0, b1, b2 = packed[i], packed[i+1], packed[i+2]
        ch0.append( b0 | ((b1 & 0x0F) << 8) )          # lower 12 bits → AIN4
        ch1.append( ((b1 >> 4) & 0x0F) | (b2 << 4) )  # upper 12 bits → AIN5


async def ble_task() -> None:
    print(f"Scanning for '{DEVICE_NAME}' …")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if device is None:
        print("[ERROR] device not found — check DEVICE_NAME matches prj.conf"); return

    print(f"Found {device.name} [{device.address}] — connecting …")
    async with BleakClient(device) as client:
        await client.start_notify(NUS_TX_CHAR_UUID, on_notify)
        print("Subscribed. Streaming …\n")
        await asyncio.get_event_loop().create_future()

def ble_thread() -> None:
    asyncio.run(ble_task())


fig, ax = plt.subplots(1, 1)
line0, = ax.plot([], [], lw=0.8)
ax.set_ylabel("AIN4 (raw 12-bit)")
ax.set_ylim(0, 4096)

def update(_frame) -> tuple:
    xs = range(len(ch0))
    line0.set_data(xs, list(ch0))
    ax.set_xlim(0, max(1, len(ch0)))
    return line0,


threading.Thread(target=ble_thread, daemon=True).start()
ani = animation.FuncAnimation(fig, update, interval=50, cache_frame_data=False)
plt.tight_layout()
plt.show()