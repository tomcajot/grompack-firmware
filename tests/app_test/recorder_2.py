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
MAX_REC_SECONDS = 100   # recording hard cap; change freely

HISTORY_SECONDS = 10 
MAX_HISTORY     = SAMPLE_RATE * HISTORY_SECONDS # Total points kept in memory (125,000)
VIEW_WINDOW     = SAMPLE_RATE * 1               # How many points to show on screen at once


# ── Shared state ──────────────────────────────────────────────────────────────
ch2_data   = deque(maxlen=MAX_POINTS)   # display ring buffer
ch2_x    = deque(maxlen=MAX_HISTORY)   # NEW: Track absolute sample indices
global_sample_count = 0                # NEW: Running counter

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
        arr = (np.array(chunk, dtype=np.int32) * 16 - 32768).astype(np.int16)
        _wav_file.writeframes(arr.tobytes())
        _samples_saved += len(chunk)
        if _samples_saved >= _max_samples:
            _wav_file.close()
            _wav_file = None
            print(f"Recording complete ({MAX_REC_SECONDS}s cap reached).")


# ── BLE callback / async task ─────────────────────────────────────────────────

def on_notify(_handle: int, data: bytearray) -> None:
    global global_sample_count
    
    if len(data) < PACKET_SIZE:
        return

    _index, packed = struct.unpack_from(PACKET_FORMAT, data)

    ch2_batch = []
    for i in range(0, PACKED_BUFFER_SIZE, 3):
        b0, b1, b2 = packed[i], packed[i + 1], packed[i + 2]
        s2 = ((b1 >> 4) | (b2 << 4)) & 0x0FFF
        
        ch2_data.append(s2)
        ch2_x.append(global_sample_count) # Track the X coordinate
        global_sample_count += 1
        
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

# ── pyqtgraph UI ──────────────────────────────────────────────────────────────
def build_ui():
    pg.setConfigOptions(antialias=False)

    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)

    # 1. Create a main window to hold both the plot and the UI controls
    main_win = QtWidgets.QWidget()
    main_win.setWindowTitle(f"Live BLE — {DEVICE_NAME}")
    main_win.resize(1100, 400)
    main_win.setStyleSheet("background-color: #1e1e1e; color: white;")
    
    layout = QtWidgets.QVBoxLayout(main_win)
    layout.setContentsMargins(5, 5, 5, 5)

    # 2. Add an Auto-Scroll Checkbox
    chk_autoscroll = QtWidgets.QCheckBox(" Auto-scroll to newest data (Uncheck to pan/zoom history)")
    chk_autoscroll.setChecked(True)
    layout.addWidget(chk_autoscroll)

    # 3. Setup the pyqtgraph widget
    win = pg.GraphicsLayoutWidget()
    layout.addWidget(win)

    p = win.addPlot(row=0, col=0, title="CH2")
    p.setLabel("left", "Raw ADC")
    p.setLabel("bottom", "Sample Index")
    p.showGrid(x=True, y=True, alpha=0.3)
    p.getAxis("left").setTextPen("w")
    p.getAxis("bottom").setTextPen("w")
    
    # Crucial for performance with large data arrays
    p.setDownsampling(mode='peak')
    p.setClipToView(True)
    
    curve = p.plot(pen=pg.mkPen("#ff8a65", width=1))

    # 4. The Update Loop
    def update():
        if len(ch2_data) == 0:
            return

        x = np.array(ch2_x, dtype=np.float32)
        y = np.array(ch2_data, dtype=np.float32)
        curve.setData(x, y)

        # If auto-scroll is checked, lock the X-axis to the newest data
        if chk_autoscroll.isChecked():
            latest_x = x[-1]
            p.setXRange(latest_x - VIEW_WINDOW, latest_x, padding=0)

    timer = QtCore.QTimer()
    timer.timeout.connect(update)
    timer.start(20)   # 50 FPS

    main_win.show()
    
    # Return main_win instead of win so the event loop keeps the whole window alive
    return app, main_win, timer

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