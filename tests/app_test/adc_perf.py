#!/usr/bin/env python3
"""
GromPack adc-perf Verification Script
-------------------------------------
This script empirically validates the SAADC performance over the BLE link.
It continuously calculates the effective sampling frequency (f_s) and verifies 
the 12-bit dynamic range integrity (quantization levels) over rolling 1-second windows.
"""

import asyncio
import struct
import time
from bleak import BleakClient, BleakScanner

# ── Configuration ─────────────────────────────────────────────────────────────
DEVICE_NAME        = "grompack"
NUS_SERVICE_UUID   = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_CHAR_UUID   = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID   = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

# Packet structure: 4-byte index + 240-byte interleaved payload
PACKED_BUFFER_SIZE = 240
PACKET_FORMAT      = f"<I{PACKED_BUFFER_SIZE}s"
PACKET_SIZE        = struct.calcsize(PACKET_FORMAT)
SAMPLES_PER_PACKET = 80  # 240 bytes / 3 bytes per pair

TARGET_CHANNEL     = 2  # Set to 1 or 2 for isolated verification

# ── Global State ──────────────────────────────────────────────────────────────
stats = {
    "sample_count": 0,
    "values": set(),  # Mathematical set to guarantee unique quantization bins
    "min_val": 4096,  # Theoretical max of 12-bit ADC + 1
    "max_val": -1,
    "last_index": None,
    "dropped_packets": 0
}

# ── BLE Notification Callback ─────────────────────────────────────────────────
def on_notify(_handle: int, data: bytearray) -> None:
    if len(data) < PACKET_SIZE:
        return

    # Extract the 4-byte sequence index and the 240-byte payload
    _index, packed = struct.unpack_from(PACKET_FORMAT, data)

    # --- 1. Sequence Continuity Verification ---
    # The firmware assigns the global_sample_counter to _index. 
    # Therefore, _index jumps by SAMPLES_PER_PACKET (80) per transmission.
    if stats["last_index"] is not None:
        if _index > stats["last_index"]:
            diff = _index - stats["last_index"]
            if diff > SAMPLES_PER_PACKET:
                # Calculate exactly how many 80-sample packets were lost
                packets_lost = (diff // SAMPLES_PER_PACKET) - 1
                stats["dropped_packets"] += packets_lost
                print(f"\n[WARNING] Link instability! Dropped {packets_lost} packet(s).")
        elif _index < stats["last_index"]:
            print(f"\n[INFO] Index reset detected (MCU likely restarted).")

    stats["last_index"] = _index

    # --- 2. Data Extraction & Quantization Tracking ---
    # The payload packs two 12-bit samples into every 3 bytes to eliminate overhead.
    for i in range(0, PACKED_BUFFER_SIZE, 3):
        b0, b1, b2 = packed[i], packed[i + 1], packed[i + 2]
        
        if TARGET_CHANNEL == 1:
            s = b0 | ((b1 & 0x0F) << 8)
        else:
            s = ((b1 >> 4) | (b2 << 4)) & 0x0FFF
            
        stats["sample_count"] += 1
        
        # Add sample to the set. Sets naturally discard duplicate values.
        # By the end of 1 second, the length of this set equals the exact 
        # number of unique 12-bit bins hit by the SAADC.
        stats["values"].add(s)
        
        # Track dynamic range bounds
        if s < stats["min_val"]: stats["min_val"] = s
        if s > stats["max_val"]: stats["max_val"] = s

# ── Performance Metric Calculation ────────────────────────────────────────────
async def log_metrics():
    """
    Independent asynchronous loop that calculates f_s and dynamic range 
    using high-precision hardware timers to bypass BLE burst jitter.
    """
    last_count = 0
    last_time = time.perf_counter()
    
    print(f"\n--- Monitoring SAADC CH{TARGET_CHANNEL} Performance ---")
    print("Waiting for data buffer to fill...\n")
    
    while True:
        # Evaluate metrics strictly every 1.0 wall-clock seconds
        await asyncio.sleep(1.0) 
        
        current_time = time.perf_counter()
        current_count = stats["sample_count"]
        
        # --- Effective Sampling Frequency (f_s) ---
        # Calculation: ΔSamples / ΔTime
        dt = current_time - last_time
        ds = current_count - last_count
        freq = ds / dt if dt > 0 else 0
        
        # --- Resolution & Dynamic Range Integrity ---
        # If the 12-bit data is truncated (e.g. to 10-bit), unique_levels mathematically 
        # cannot exceed 1024. If it approaches 4096 under a full-scale signal, 
        # 12-bit integrity is proven.
        unique_levels = len(stats["values"])
        min_v = stats["min_val"] if stats["min_val"] != 4096 else 0
        max_v = stats["max_val"] if stats["max_val"] != -1 else 0
        
        # Output formatting matches the academic methodology requirements
        print(f"[CH{TARGET_CHANNEL}] f_s: {freq:8.2f} Hz | "
              f"Unique Bins: {unique_levels:4}/4096 | "
              f"Range: [{min_v:4}, {max_v:4}] | "
              f"Dropped Packets: {stats['dropped_packets']}")
        
        # Reset the quantization tracking set and bounds for the next 1-second window
        stats["values"].clear()
        stats["min_val"] = 4096
        stats["max_val"] = -1
        
        last_count = current_count
        last_time = current_time

# ── Main Async Execution ──────────────────────────────────────────────────────
async def ble_task() -> None:
    print(f"Scanning for '{DEVICE_NAME}' …")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)

    if device is None:
        print("[ERROR] Device not found. Ensure the board is advertising.")
        return

    print(f"Found {device.name} [{device.address}] — Establishing connection…")

    async with BleakClient(device) as client:
        nus = next((s for s in client.services if s.uuid.lower() == NUS_SERVICE_UUID.lower()), None)
        if not nus:
            print("[ERROR] NUS service not found"); return

        tx = next((c for c in nus.characteristics if c.uuid.lower() == NUS_TX_CHAR_UUID.lower()), None)
        rx = next((c for c in nus.characteristics if c.uuid.lower() == NUS_RX_CHAR_UUID.lower()), None)

        if not tx or not rx:
            print("[ERROR] TX/RX characteristic not found"); return

        # Subscribe to notifications and trigger the start command
        await client.start_notify(tx, on_notify)
        await client.write_gatt_char(rx, b'\x01')

        # Run the metric logging loop concurrently with the BLE listener
        await log_metrics()

if __name__ == "__main__":
    try:
        asyncio.run(ble_task())
    except KeyboardInterrupt:
        print("\nMonitoring terminated by user. Finalizing logs.")