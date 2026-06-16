#!/usr/bin/env python3
"""
GromPack adc-perf Live Dashboard & Summarizer
---------------------------------------------
Calculates effective sampling frequency (f_s) and verifies 12-bit 
dynamic range integrity dynamically in the terminal.
Generates a comprehensive summary report upon exit for academic validation.
"""

import sys
import math
import asyncio
import struct
import time
from bleak import BleakClient, BleakScanner

# ── Configuration ─────────────────────────────────────────────────────────────
DEVICE_NAME        = "grompack"
NUS_SERVICE_UUID   = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_CHAR_UUID   = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID   = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

PACKED_BUFFER_SIZE = 240
PACKET_FORMAT      = f"<I{PACKED_BUFFER_SIZE}s"
PACKET_SIZE        = struct.calcsize(PACKET_FORMAT)
SAMPLES_PER_PACKET = 80  # 240 bytes / 3 bytes per pair

TARGET_CHANNEL     = 2   # Set to 1 or 2 

# ── Global State & History ────────────────────────────────────────────────────
stats = {
    "sample_count": 0,
    "total_samples_processed": 0,
    "values": set(),  
    "min_val": 4096,  
    "max_val": -1,
    "last_index": None,
    "dropped_packets": 0,
    "mcu_resets": 0
}

# Historical tracking for final report
history = {
    "frequencies": [],
    "unique_bins": []
}

# ── BLE Notification Callback ─────────────────────────────────────────────────
def on_notify(_handle: int, data: bytearray) -> None:
    if len(data) < PACKET_SIZE:
        return

    _index, packed = struct.unpack_from(PACKET_FORMAT, data)

    # Sequence Continuity Verification (80 samples per packet)
    if stats["last_index"] is not None:
        if _index > stats["last_index"]:
            diff = _index - stats["last_index"]
            if diff > SAMPLES_PER_PACKET:
                stats["dropped_packets"] += (diff // SAMPLES_PER_PACKET) - 1
        elif _index < stats["last_index"]:
            stats["mcu_resets"] += 1

    stats["last_index"] = _index

    # Data Extraction & Quantization Tracking
    for i in range(0, PACKED_BUFFER_SIZE, 3):
        b0, b1, b2 = packed[i], packed[i + 1], packed[i + 2]
        
        if TARGET_CHANNEL == 1:
            s = b0 | ((b1 & 0x0F) << 8)
        else:
            s = ((b1 >> 4) | (b2 << 4)) & 0x0FFF
            
        stats["sample_count"] += 1
        stats["total_samples_processed"] += 1
        stats["values"].add(s)
        
        if s < stats["min_val"]: stats["min_val"] = s
        if s > stats["max_val"]: stats["max_val"] = s

# ── Live Terminal UI Dashboard ────────────────────────────────────────────────
async def display_loop():
    last_count = 0
    last_time = time.perf_counter()
    
    try:
        while True:
            await asyncio.sleep(1.0) 
            
            current_time = time.perf_counter()
            current_count = stats["sample_count"]
            
            dt = current_time - last_time
            ds = current_count - last_count
            freq = ds / dt if dt > 0 else 0
            
            unique_levels = len(stats["values"])
            min_v = stats["min_val"] if stats["min_val"] != 4096 else 0
            max_v = stats["max_val"] if stats["max_val"] != -1 else 0
            
            # Only record history if we actually received data
            if ds > 0:
                history["frequencies"].append(freq)
                history["unique_bins"].append(unique_levels)
            
            # Terminal Dashboard
            sys.stdout.write("\033[H\033[J")
            sys.stdout.write("==================================================\n")
            sys.stdout.write(f" 🎛️  SAADC Performance Monitor — CH{TARGET_CHANNEL}\n")
            sys.stdout.write("==================================================\n")
            sys.stdout.write(f" Effective Sampling Rate: {freq:8.2f} Hz\n")
            sys.stdout.write(f" Unique 12-bit Bins Hit:  {unique_levels:4} / 4096\n")
            sys.stdout.write(f" Dynamic Range Bounds:    [{min_v:4}, {max_v:4}]\n")
            sys.stdout.write("--------------------------------------------------\n")
            sys.stdout.write(f" Link Dropped Packets:    {stats['dropped_packets']}\n")
            sys.stdout.write(f" MCU Resets Detected:     {stats['mcu_resets']}\n")
            sys.stdout.write(f" Last Packet Index:       {stats['last_index']}\n")
            sys.stdout.write("==================================================\n")
            sys.stdout.write(" Action: Press [Ctrl+C] to exit safely and get report.\n")
            sys.stdout.flush()
            
            # Reset window stats
            stats["values"].clear()
            stats["min_val"] = 4096
            stats["max_val"] = -1
            
            last_count = current_count
            last_time = current_time
            
    except asyncio.CancelledError:
        pass

# ── Main Async Execution ──────────────────────────────────────────────────────
async def ble_task() -> None:
    print(f"Scanning for '{DEVICE_NAME}' …")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)

    if device is None:
        print("\n❌ [ERROR] Device not found. Ensure the board is advertising.")
        return

    print(f"Found {device.name} [{device.address}] — Establishing connection…")

    async with BleakClient(device) as client:
        nus = next((s for s in client.services if s.uuid.lower() == NUS_SERVICE_UUID.lower()), None)
        if not nus:
            print("\n❌ [ERROR] NUS service not found"); return

        tx = next((c for c in nus.characteristics if c.uuid.lower() == NUS_TX_CHAR_UUID.lower()), None)
        rx = next((c for c in nus.characteristics if c.uuid.lower() == NUS_RX_CHAR_UUID.lower()), None)

        if not tx or not rx:
            print("\n❌ [ERROR] TX/RX characteristic not found"); return

        await client.start_notify(tx, on_notify)
        await client.write_gatt_char(rx, b'\x01')

        display_task = asyncio.create_task(display_loop())

        try:
            while True:
                await asyncio.sleep(1)
        except (KeyboardInterrupt, asyncio.CancelledError):
            pass
        finally:
            display_task.cancel()
            await display_task
            print("\nStopping hardware pipeline & subscriptions...")
            try:
                await client.write_gatt_char(rx, b'\x02')
                await client.stop_notify(tx)
            except Exception:
                pass

def print_final_report():
    print("\n\n==================================================")
    print(" 📋 FINAL SAADC PERFORMANCE SUMMARY REPORT")
    print("==================================================")
    print(f" Total Samples Processed: {stats['total_samples_processed']}")
    print(f" Total Packets Dropped:   {stats['dropped_packets']}")
    print(f" MCU Resets Registered:   {stats['mcu_resets']}")
    print("--------------------------------------------------")
    
    if history["frequencies"]:
        min_f = min(history["frequencies"])
        max_f = max(history["frequencies"])
        avg_f = sum(history["frequencies"]) / len(history["frequencies"])
        print(f" Min Frequency (f_s):     {min_f:8.2f} Hz")
        print(f" Max Frequency (f_s):     {max_f:8.2f} Hz")
        print(f" Avg Frequency (f_s):     {avg_f:8.2f} Hz")
    else:
        print(" No frequency data collected.")
        
    print("--------------------------------------------------")
    
    if history["unique_bins"]:
        min_b = min(history["unique_bins"])
        max_b = max(history["unique_bins"])
        avg_b = sum(history["unique_bins"]) / len(history["unique_bins"])
        eff_res = math.log2(avg_b) if avg_b > 0 else 0.0
        
        print(f" Min Unique Bins:         {min_b:4} / 4096")
        print(f" Max Unique Bins:         {max_b:4} / 4096")
        print(f" Avg Unique Bins:         {avg_b:7.2f} / 4096")
        print(f" Effective Resolution:    {eff_res:4.2f} bits")
    else:
        print(" No bin data collected.")
        
    print("==================================================\n")

if __name__ == "__main__":
    print("\033[2J\033[H", end="")
    try:
        asyncio.run(ble_task())
    except KeyboardInterrupt:
        pass
    finally:
        print_final_report()