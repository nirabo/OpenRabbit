#!/usr/bin/env python3
"""Capture serial output from the RCM5700 programming port.

Unlike a plain `cat /dev/ttyUSB0`, this drives DTR/RTS itself so the port does
not hold the target in reset. With -r it pulses DTR to reset the module first
(which, with the JP1 1-2 jumper removed, makes it boot from flash).

Usage: serialcap.py [-r] [-b BAUD] [-t SECONDS] [DEVICE]
"""
import argparse
import sys
import time

import serial

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("device", nargs="?", default="/dev/ttyUSB0")
    ap.add_argument("-b", "--baud", type=int, default=38400)
    ap.add_argument("-t", "--time", type=float, default=8.0)
    ap.add_argument("-r", "--reset", action="store_true",
                    help="pulse DTR to reset the module before capturing")
    args = ap.parse_args()

    s = serial.Serial()
    s.port = args.device
    s.baudrate = args.baud
    s.timeout = 0.2
    s.dtr = False
    s.rts = False
    s.open()

    if args.reset:
        s.dtr = True
        time.sleep(0.3)
        s.dtr = False
        time.sleep(0.3)

    total = 0
    end = time.time() + args.time
    try:
        while time.time() < end:
            data = s.read(256)
            if data:
                total += len(data)
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
    except KeyboardInterrupt:
        pass
    finally:
        s.close()
    sys.stderr.write("\n[serialcap: %d bytes]\n" % total)

if __name__ == "__main__":
    main()
