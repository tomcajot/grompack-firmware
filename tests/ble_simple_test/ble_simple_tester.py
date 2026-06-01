import asyncio
import struct
from bleak import BleakClient, BleakScanner

DEVICE_NAME       = "grompack"
NUS_TX_CHAR_UUID  = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NUS_SERVICE_UUID  = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID  = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
PACKED_BUFFER_SIZE = 240
PACKET_FORMAT      = f"<I{PACKED_BUFFER_SIZE}s"
PACKET_SIZE        = struct.calcsize(PACKET_FORMAT)

def on_notify(_handle: int, data: bytearray) -> None:
    if len(data) < PACKET_SIZE:
        return

    _index, packed = struct.unpack_from(PACKET_FORMAT, data)

    ch1_samples = []
    ch2_samples = []

    for i in range(0, 120, 3):
        b0, b1, b2 = packed[i], packed[i+1], packed[i+2]
        
        samp1_A = (b0 << 4) | (b1 >> 4)
        samp1_B = ((b1 & 0x0F) << 8) | b2
        
        ch1_samples.extend([samp1_A, samp1_B])

    for i in range(0, 120, 3):
        b0, b1, b2 = packed[120+i], packed[120+i+1], packed[120+i+2]
        
        samp2_A = (b0 << 4) | (b1 >> 4)
        samp2_B = ((b1 & 0x0F) << 8) | b2
        
        ch2_samples.extend([samp2_A, samp2_B])

    print(f"Packet Index: {_index}")
    print(f"CH1: {ch1_samples}")
    print(f"CH2: {ch2_samples}")
    print("-" * 60)


async def ble_task() -> None:
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

        print(f"Using TX char at handle {tx_char.handle}")
        await client.start_notify(tx_char, on_notify)
        print("Subscribed. Streaming … (Ctrl+C to stop)\n")
        await asyncio.Future()

if __name__ == "__main__":
    try:
        asyncio.run(ble_task())
    except KeyboardInterrupt:
        print("\nStreaming stopped by user.")
