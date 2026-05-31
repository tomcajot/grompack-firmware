#!/usr/bin/env python3

import sys
import pylink
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import re

DEVICE = "nRF54L15_M33"
MAX_POINTS = 50

ansi_escape = re.compile(r'\x1b\[[0-9;]*m')

jlink = pylink.JLink()
jlink.open()
jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
jlink.connect(DEVICE)
jlink.rtt_start()

print(f"Connected to {DEVICE} over rtt\n")

fig, ax = plt.subplots()
values = deque(maxlen=MAX_POINTS)
line, = ax.plot([], [], lw=1.5)

ax.set_title("RTT Data")
ax.set_ylabel("Raw ADC Value")
ax.set_xlabel(f"Most recent {MAX_POINTS} samples")
ax.set_xlim(0, MAX_POINTS)

leftover_string = ""

# i = 0

def update(frame):
    global leftover_string
    # global i
    
    new_data_added = False
    
    while True:
        data = jlink.rtt_read(0, 4096)
        if not data:
            break
            
        raw_str = bytes(data).decode("utf-8", errors="ignore")
        leftover_string += raw_str
        new_data_added = True

    if new_data_added:
        leftover_string = leftover_string.replace('\r\n', '\n').replace('\r', '\n')
        lines = leftover_string.split('\n')
        
        leftover_string = lines.pop()
        
        for line_str in lines:
            clean_line = ansi_escape.sub("", line_str).lower()
            
            if "saadc" in clean_line:
                try:
                    val = int(clean_line.split("saadc")[1].strip())
                    # i += 1
                    # if ((i % 2) == 0):
                    values.append(val)
                except (ValueError, IndexError):
                    pass

        if values:
            line.set_data(range(len(values)), list(values))
            ax.relim()
            ax.autoscale_view(scalex=False, scaley=True)

    return line,

ani = animation.FuncAnimation(fig, update, interval=50, cache_frame_data=False)

try:
    plt.show()
except KeyboardInterrupt:
    pass
finally:
    jlink.rtt_stop()
    jlink.close()
    print("\nDisconnected.")