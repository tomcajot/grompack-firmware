#!/usr/bin/env python3
"""
BLE real-time visualiser — pyqtgraph edition
• Shows CH2 only
• Records CH2 to a WAV file (16-bit PCM, mono) up to MAX_REC_SECONDS

Dependencies:
    pip install bleak pyqtgraph PyQt6 numpy
    (PyQt5 also works — pyqtgraph will use whichever Qt binding is installed)
"""

import sys
import wave
import asyncio
import struct
import threading
from collections import deque
from datetime import datetime

import numpy as np
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore, QtWidgets

from bleak import BleakClient, BleakScanner

# ── Config ────────────────────────────────────────────────────────────────────
DEVICE_NAME        = "grompack"
NUS_SERVICE_UUID   = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_CHAR_UUID   = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID   = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

PACKED_BUFFER_SIZE = 240
PACKET_FORMAT      = f"<I{PACKED_BUFFER_SIZE}s"
PACKET_SIZE        = struct.calcsize(PACKET_FORMAT)

MAX_POINTS      = 500   # rolling window shown on screen (samples)
SAMPLE_RATE     = 12500  # Hz — set to your device's actual sample rate
MAX_REC_SECONDS = 10   # recording hard cap; change freely

# ── Shared state ──────────────────────────────────────────────────────────────
ch2_data   = deque(maxlen=MAX_POINTS)   # display ring buffer
is_running = True

# WAV writer (opened once, written from the BLE callback thread)
_wav_file      = None
_wav_lock      = threading.Lock()
_samples_saved = 0
_max_samples   = SAMPLE_RATE * MAX_REC_SECONDS


# ── WAV helpers ───────────────────────────────────────────────────────────────

def _open_wav() -> wave.Wave_write:
    filename = datetime.now().strftime("rec_%Y%m%d_%H%M%S.wav")
    w = wave.open(filename, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)          # 16-bit
    w.setframerate(SAMPLE_RATE)
    print(f"Recording CH2 → {filename}  (max {MAX_REC_SECONDS}s)")
    return w


def _write_samples(samples: list[int]) -> None:
    """Append 12-bit ADC samples (0–4095) scaled to int16 WAV frames."""
    global _samples_saved, _wav_file
    with _wav_lock:
        if _wav_file is None or _samples_saved >= _max_samples:
            return
        remaining = _max_samples - _samples_saved
        chunk = samples[:remaining]
        # scale 12-bit (0–4095) → int16 (−32768–32767)
        arr = (np.array(chunk, dtype=np.int32) * 16 - 32768).astype(np.int16)
        _wav_file.writeframes(arr.tobytes())
        _samples_saved += len(chunk)
        if _samples_saved >= _max_samples:
            _wav_file.close()
            _wav_file = None
            print(f"Recording complete ({MAX_REC_SECONDS}s cap reached).")


# ── BLE callback / async task ─────────────────────────────────────────────────

def on_notify(_handle: int, data: bytearray) -> None:
    if len(data) < PACKET_SIZE:
        return

    _index, packed = struct.unpack_from(PACKET_FORMAT, data)

    ch2_batch = []
    for i in range(0, PACKED_BUFFER_SIZE, 3):
        b0, b1, b2 = packed[i], packed[i + 1], packed[i + 2]
        s2 = ((b1 >> 4) | (b2 << 4)) & 0x0FFF
        ch2_data.append(s2)
        ch2_batch.append(s2)

    _write_samples(ch2_batch)


async def ble_task() -> None:
    global is_running
    print(f"Scanning for '{DEVICE_NAME}' …")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)

    if device is None:
        print("[ERROR] device not found")
        return

    print(f"Found {device.name} [{device.address}] — connecting …")

    async with BleakClient(device) as client:
        nus = next(
            (s for s in client.services
             if s.uuid.lower() == NUS_SERVICE_UUID.lower()), None
        )
        if not nus:
            print("[ERROR] NUS service not found"); return

        tx = next((c for c in nus.characteristics
                   if c.uuid.lower() == NUS_TX_CHAR_UUID.lower()), None)
        rx = next((c for c in nus.characteristics
                   if c.uuid.lower() == NUS_RX_CHAR_UUID.lower()), None)

        if not tx or not rx:
            print("[ERROR] TX/RX characteristic not found"); return

        print(f"Using TX char at handle {tx.handle}")
        await client.start_notify(tx, on_notify)
        await client.write_gatt_char(rx, b'\x01')

        while is_running:
            await asyncio.sleep(0.05)

        print("Stopping BLE notifications …")
        await client.stop_notify(tx)


def start_ble_background():
    asyncio.run(ble_task())


# ── pyqtgraph UI ──────────────────────────────────────────────────────────────

def build_ui():
    pg.setConfigOptions(antialias=False)

    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)

    win = pg.GraphicsLayoutWidget(title=f"Live BLE — {DEVICE_NAME}")
    win.resize(1100, 400)
    win.setBackground("#1e1e1e")

    p = win.addPlot(row=0, col=0, title="CH2")
    p.setLabel("left",   "Raw ADC")
    p.setLabel("bottom", f"Last {MAX_POINTS} samples")
    p.setXRange(0, MAX_POINTS, padding=0)
    p.showGrid(x=True, y=True, alpha=0.3)
    p.getAxis("left").setTextPen("w")
    p.getAxis("bottom").setTextPen("w")
    p.titleLabel.setText("CH2", color="w", size="11pt")
    curve = p.plot(np.zeros(MAX_POINTS), pen=pg.mkPen("#ff8a65", width=1))

    def update():
        d = np.array(ch2_data, dtype=np.float32)
        if d.size:
            curve.setData(d)

    timer = QtCore.QTimer()
    timer.timeout.connect(update)
    timer.start(20)   # 50 FPS

    win.show()
    return app, win, timer


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    _wav_file = _open_wav()

    ble_thread = threading.Thread(target=start_ble_background, daemon=True)
    ble_thread.start()

    app, win, timer = build_ui()

    try:
        app.exec()
    finally:
        print("\nWindow closed — shutting down …")
        is_running = False
        ble_thread.join(timeout=3)
        with _wav_lock:
            if _wav_file is not None:
                _wav_file.close()
                print(f"WAV saved ({_samples_saved} samples).")
        print("Done.")