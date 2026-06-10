#!/usr/bin/env python3
"""
BLE Performance Monitor CLI
• Measures packet-to-packet timing gaps (Max, Min, and Avg intervals)
• Tracks packet drops/loss rate using the struct sample index
• Refreshes real-time stats via a clean terminal dashboard
"""

import sys
import asyncio
import struct
import time
from bleak import BleakClient, BleakScanner

# ── Config (Derived from your source files) ───────────────────────────────────
DEVICE_NAME        = "grompack"
NUS_SERVICE_UUID   = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_CHAR_UUID   = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID   = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

PACKED_BUFFER_SIZE = 240
PACKET_FORMAT      = f"<I{PACKED_BUFFER_SIZE}s"
PACKET_SIZE        = struct.calcsize(PACKET_FORMAT)
SAMPLES_PER_PACKET = 80  # 240 bytes / 3 bytes per pair

# ── Performance Metrics State ──────────────────────────────────────────────────
total_received   = 0
total_missed     = 0
mcu_resets       = 0
last_index       = None

last_packet_time = None
max_time_delta   = 0.0
min_time_delta   = float('inf')
total_time_delta = 0.0
delta_count      = 0

# ── BLE Notification Callback ─────────────────────────────────────────────────
def on_notify(_handle: int, data: bytearray) -> None:
    global total_received, total_missed, mcu_resets, last_index
    global last_packet_time, max_time_delta, min_time_delta, total_time_delta, delta_count

    if len(data) < PACKET_SIZE:
        return

    # 1. Capture high-resolution arrival time immediately
    current_time = time.perf_counter()
    total_received += 1

    # Unpack sample_index
    index, _ = struct.unpack_from(PACKET_FORMAT, data)

    # 2. Timing Gap Calculations
    if last_packet_time is not None:
        delta = current_time - last_packet_time
        total_time_delta += delta
        delta_count += 1
        if delta > max_time_delta:
            max_time_delta = delta
        if delta < min_time_delta:
            min_time_delta = delta
    last_packet_time = current_time

    # 3. Packet Loss Analysis via Index Delta
    if last_index is not None:
        if index > last_index:
            diff = index - last_index
            if diff > SAMPLES_PER_PACKET:
                missed = (diff // SAMPLES_PER_PACKET) - 1
                total_missed += missed
        elif index < last_index:
            # Index decreased: indicates MCU reset or a hardware power-cycle
            mcu_resets += 1

    last_index = index

# ── Live Terminal UI Dashboard ────────────────────────────────────────────────
async def display_loop():
    """Periodically prints live stats to the terminal dashboard."""
    try:
        while True:
            # Clear terminal screen and reset cursor position to home
            sys.stdout.write("\033[H\033[J")
            
            avg_delta_ms = (total_time_delta / delta_count * 1000) if delta_count > 0 else 0.0
            max_delta_ms = max_time_delta * 1000
            min_delta_ms = (min_time_delta * 1000) if min_time_delta != float('inf') else 0.0
            
            total_expected = total_received + total_missed
            loss_rate = (total_missed / total_expected * 100) if total_expected > 0 else 0.0

            sys.stdout.write("==================================================\n")
            sys.stdout.write(f" 📊 BLE Performance Monitor — Device: '{DEVICE_NAME}'\n")
            sys.stdout.write("==================================================\n")
            sys.stdout.write(f" Connection Status:   🟢 RUNNING & STREAMING\n")
            sys.stdout.write(f" Packets Received:    {total_received}\n")
            sys.stdout.write(f" Packets Missed:      {total_missed}\n")
            sys.stdout.write(f" Packet Loss Rate:    {loss_rate:.2f}%\n")
            sys.stdout.write(f" MCU Resets Detected: {mcu_resets}\n")
            sys.stdout.write("--------------------------------------------------\n")
            sys.stdout.write(f" Last Packet Index:   {last_index if last_index is not None else 'N/A'}\n")
            sys.stdout.write(f" Min Arrival Gap:     {min_delta_ms:.2f} ms\n")
            sys.stdout.write(f" Max Arrival Gap:     {max_delta_ms:.2f} ms\n")
            sys.stdout.write(f" Avg Arrival Gap:     {avg_delta_ms:.2f} ms\n")
            sys.stdout.write("==================================================\n")
            sys.stdout.write(" Action: Press [Ctrl+C] to exit safely and get report.\n")
            sys.stdout.flush()
            
            await asyncio.sleep(0.1)  # Refresh 10 times per second
    except asyncio.CancelledError:
        pass

# ── Async Application Core ────────────────────────────────────────────────────
async def main():
    print(f"Scanning for device named '{DEVICE_NAME}'...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)

    if device is None:
        print(f"❌ [ERROR] Device '{DEVICE_NAME}' not found.")
        return

    print(f"Found {device.name} [{device.address}] — Connecting...")

    async with BleakClient(device) as client:
        nus = next((s for s in client.services if s.uuid.lower() == NUS_SERVICE_UUID.lower()), None)
        if not nus:
            print("❌ [ERROR] Nordic UART Service (NUS) not found.")
            return

        tx = next((c for c in nus.characteristics if c.uuid.lower() == NUS_TX_CHAR_UUID.lower()), None)
        rx = next((c for c in nus.characteristics if c.uuid.lower() == NUS_RX_CHAR_UUID.lower()), None)

        if not tx or not rx:
            print("❌ [ERROR] NUS TX/RX characteristics missing.")
            return

        # Start listening to notifications
        await client.start_notify(tx, on_notify)
        # Send start command (0x01) to command_queue to boot the pipeline
        await client.write_gatt_char(rx, b'\x01')

        # Start the UI dashboard task
        display_task = asyncio.create_task(display_loop())

        try:
            # Keep the main loop running infinitely
            while True:
                await asyncio.sleep(1)
        except (KeyboardInterrupt, asyncio.CancelledError):
            pass
        finally:
            # Gracefully clean up threads and cancel notifications
            display_task.cancel()
            await display_task
            print("\nStopping hardware pipeline & subscriptions...")
            try:
                # Sends stop command implicitly when disconnected or manually here
                await client.write_gatt_char(rx, b'\x02')
                await client.stop_notify(tx)
            except Exception:
                pass

if __name__ == "__main__":
    # Clear screen initially
    print("\033[2J\033[H", end="")
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        # Final Summary Printout
        print("\n\n==================================================")
        print(" 📋 FINAL BLE PERFORMANCE SUMMARY REPORT")
        print("==================================================")
        print(f" Total Packets Received: {total_received}")
        print(f" Total Packets Missed:   {total_missed}")
        total_expected = total_received + total_missed
        loss_rate = (total_missed / total_expected * 100) if total_expected > 0 else 0.0
        print(f" Final Packet Loss Rate: {loss_rate:.2f}%")
        print(f" MCU Resets Registered:  {mcu_resets}")
        print("--------------------------------------------------")
        max_delta_ms = max_time_delta * 1000
        avg_delta_ms = (total_time_delta / delta_count * 1000) if delta_count > 0 else 0.0
        print(f" Worst-case Packet Gap:  {max_delta_ms:.2f} ms")
        print(f" Nominal (Avg) Gap:      {avg_delta_ms:.2f} ms")
        print("==================================================")
        print("Done. Session closed cleanly.")