#!/usr/bin/env python3

import time
import sys
import pylink

DEVICE = "nrf54l15"

jlink = pylink.JLink()
jlink.open()
jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
jlink.connect(DEVICE)
jlink.rtt_start()

print(f"Connected to {DEVICE} over rtt\n")

try:
    while True:
        data = jlink.rtt_read(0, 1024)
        if data:
            sys.stdout.write(bytes(data).decode("utf-8", errors="replace"))
            sys.stdout.flush()
        else:
            time.sleep(0.01)
except KeyboardInterrupt:
    pass
finally:
    jlink.rtt_stop()
    jlink.close()
    print("\nDisconnected.")