#!/usr/bin/env python3
"""
BLE real-time visualiser — pyqtgraph edition
• Shows CH1 or CH2 (Togglable via UI Button)
• Records the active channel to a WAV file (16-bit PCM, mono)
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

MAX_POINTS      = 10000   # rolling window shown on screen (samples)
SAMPLE_RATE     = 12500   # Hz
MAX_REC_SECONDS = 100     # recording hard cap

# ── Shared state ──────────────────────────────────────────────────────────────
TARGET_CHANNEL = 2        # Starts on CH2 by default
plot_data  = deque(maxlen=MAX_POINTS)
is_running = True

# WAV writer state
_wav_file      = None
_wav_lock      = threading.Lock()
_samples_saved = 0
_max_samples   = SAMPLE_RATE * MAX_REC_SECONDS


# ── WAV helpers ───────────────────────────────────────────────────────────────

def _open_wav() -> wave.Wave_write:
    filename = datetime.now().strftime(f"rec_ch{TARGET_CHANNEL}_%Y%m%d_%H%M%S.wav")
    w = wave.open(filename, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)          # 16-bit
    w.setframerate(SAMPLE_RATE)
    print(f"Recording CH{TARGET_CHANNEL} → {filename}  (max {MAX_REC_SECONDS}s)")
    return w

def restart_wav_recording():
    """Safely close the current WAV and start a new one."""
    global _wav_file, _samples_saved
    with _wav_lock:
        if _wav_file is not None:
            _wav_file.close()
            print(f"Saved previous recording ({_samples_saved} samples).")
        _samples_saved = 0
        _wav_file = _open_wav()

def _write_samples(samples: list[int]) -> None:
    global _samples_saved, _wav_file
    with _wav_lock:
        if _wav_file is None or _samples_saved >= _max_samples:
            return
        remaining = _max_samples - _samples_saved
        chunk = samples[:remaining]
        arr = (np.array(chunk, dtype=np.int32) * 64).astype(np.int16)
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
    data_batch = []
    
    # Process 5 bytes at a time (yields two CH1 samples and two CH2 samples)
    for i in range(0, PACKED_BUFFER_SIZE, 5):
        b0 = packed[i]
        b1 = packed[i + 1]
        b2 = packed[i + 2]
        b3 = packed[i + 3]
        b4 = packed[i + 4]

        # 10-bit unpacking
        s0 = b0 | ((b1 & 0x03) << 8)                 # CH1 (pair 1)
        s1 = ((b1 >> 2) & 0x3F) | ((b2 & 0x0F) << 6) # CH2 (pair 1)
        s2 = ((b2 >> 4) & 0x0F) | ((b3 & 0x3F) << 4) # CH1 (pair 2)
        s3 = ((b3 >> 6) & 0x03) | (b4 << 2)          # CH2 (pair 2)

        # Helper to convert 10-bit 2's complement to signed Python integer
        def sign_extend(val):
            return val - 1024 if (val & 0x200) else val

        s0 = sign_extend(s0)
        s1 = sign_extend(s1)
        s2 = sign_extend(s2)
        s3 = sign_extend(s3)
        
        # Route the requested channel to the graph and WAV
        if TARGET_CHANNEL == 1:
            plot_data.extend([s0, s2])
            data_batch.extend([s0, s2])
        else:
            plot_data.extend([s1, s3])
            data_batch.extend([s1, s3])

    _write_samples(data_batch)

async def ble_task() -> None:
    global is_running
    print(f"Scanning for '{DEVICE_NAME}' …")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)

    if device is None:
        print("[ERROR] device not found")
        return

    print(f"Found {device.name} [{device.address}] — connecting …")

    async with BleakClient(device) as client:
        nus = next((s for s in client.services if s.uuid.lower() == NUS_SERVICE_UUID.lower()), None)
        if not nus:
            print("[ERROR] NUS service not found"); return

        tx = next((c for c in nus.characteristics if c.uuid.lower() == NUS_TX_CHAR_UUID.lower()), None)
        rx = next((c for c in nus.characteristics if c.uuid.lower() == NUS_RX_CHAR_UUID.lower()), None)

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

    # Main Native Window
    main_win = QtWidgets.QWidget()
    main_win.setWindowTitle(f"Live BLE — {DEVICE_NAME}")
    main_win.resize(1100, 450)
    
    # Dark theme for the native window to match pyqtgraph
    main_win.setStyleSheet("background-color: #1e1e1e; color: white;")

    layout = QtWidgets.QVBoxLayout()
    main_win.setLayout(layout)

    # --- TOP CONTROL BAR ---
    top_layout = QtWidgets.QHBoxLayout()
    
    btn_toggle = QtWidgets.QPushButton(f"Switch to CH{1 if TARGET_CHANNEL==2 else 2}")
    btn_toggle.setStyleSheet("""
        QPushButton {
            background-color: #ff8a65; 
            color: #1e1e1e; 
            font-weight: bold; 
            padding: 8px 16px; 
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #ffab91;
        }
    """)
    top_layout.addWidget(btn_toggle)
    
    lbl_status = QtWidgets.QLabel(f"Currently Viewing & Recording: CH{TARGET_CHANNEL}")
    lbl_status.setStyleSheet("font-size: 14px; font-weight: bold; margin-left: 15px;")
    top_layout.addWidget(lbl_status)
    top_layout.addStretch() # Pushes everything to the left
    
    layout.addLayout(top_layout)

    # --- GRAPH WIDGET ---
    graph_win = pg.GraphicsLayoutWidget()
    layout.addWidget(graph_win)

    p = graph_win.addPlot(row=0, col=0, title=f"CH{TARGET_CHANNEL}")
    p.setLabel("left", "Raw ADC")
    
    ms_per_sample = 1000.0 / SAMPLE_RATE
    max_time_ms = MAX_POINTS * ms_per_sample
    
    p.setLabel("bottom", f"Time (ms) — Rolling {max_time_ms:.0f} ms window")
    p.setXRange(0, max_time_ms, padding=0)
    p.setYRange(0, 1024, padding=0)
    p.setMouseEnabled(x=True, y=False)
    p.showGrid(x=True, y=True, alpha=0.3)
    p.getAxis("left").setTextPen("w")
    p.getAxis("bottom").setTextPen("w")
    p.titleLabel.setText(f"CH{TARGET_CHANNEL}", color="w", size="11pt")
    
    curve = p.plot(pen=pg.mkPen("#ff8a65", width=1))

    # --- UI LOGIC ---
    def on_toggle_click():
        global TARGET_CHANNEL
        
        # Swap channel
        TARGET_CHANNEL = 1 if TARGET_CHANNEL == 2 else 2
        
        # Clear visual buffer to prevent a massive spike line between ch1 and ch2 values
        plot_data.clear()
        
        # Update UI labels
        btn_toggle.setText(f"Switch to CH{1 if TARGET_CHANNEL==2 else 2}")
        lbl_status.setText(f"Currently Viewing & Recording: CH{TARGET_CHANNEL}")
        p.titleLabel.setText(f"CH{TARGET_CHANNEL}", color="w", size="11pt")
        
        # Safely rotate the WAV file
        restart_wav_recording()

    btn_toggle.clicked.connect(on_toggle_click)

    # Pause logic (Spacebar)
    is_paused = False
    original_keyPressEvent = main_win.keyPressEvent

    def custom_keyPressEvent(event):
        nonlocal is_paused
        if event.text() == " ":
            is_paused = not is_paused
            if is_paused:
                p.titleLabel.setText(f"CH{TARGET_CHANNEL} [PAUSED]", color="#ff5252", size="11pt")
            else:
                p.titleLabel.setText(f"CH{TARGET_CHANNEL}", color="w", size="11pt")
        original_keyPressEvent(event)

    main_win.keyPressEvent = custom_keyPressEvent

    def update():
        if is_paused:
            return 
            
        d = np.array(plot_data, dtype=np.float32)
        if d.size:
            x = np.arange(d.size) * ms_per_sample
            curve.setData(x=x, y=d)

    timer = QtCore.QTimer()
    timer.timeout.connect(update)
    timer.start(20)

    main_win.show()
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