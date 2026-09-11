#!/usr/bin/env python3
"""Read the target STATUS line (wired to the cable's DSR) after a reset.

Use with rcm5700/statusbeacon.s, which raises STATUS if the CPU executes flash
offset 0. Because STATUS is a plain logic line, this test does not depend on
any serial baud rate.

  --rts run   RTS asserted  -> SMODE low  -> Run Mode (boot from flash)
  --rts prog  RTS released  -> SMODE high -> Program Mode (bootstrap)

Reset is driven by DTR (asserted = /RESET low), matching src/rabbit.c.

Usage: statusread.py [--rts run|prog] [--wait SEC] [DEVICE]
"""
import argparse
import sys
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("device", nargs="?", default="/dev/ttyUSB0")
    ap.add_argument("--rts", choices=["run", "prog"], default="run")
    ap.add_argument("--wait", type=float, default=1.0,
                    help="seconds to wait after releasing reset")
    ap.add_argument("--repeat", type=int, default=1)
    args = ap.parse_args()

    s = serial.Serial(args.device, 9600, timeout=0.2)
    s.dtr = False
    s.rts = (args.rts == "run")
    time.sleep(0.1)

    for i in range(args.repeat):
        s.dtr = True
        time.sleep(0.3)
        s.dtr = False
        time.sleep(args.wait)
        try:
            dsr = s.dsr
        except Exception as e:  # pragma: no cover
            print("cannot read DSR: %s" % e)
            s.close()
            return 2
        print("run %d: STATUS (DSR) = %s" % (i + 1, "HIGH" if dsr else "low"))
        time.sleep(0.2)

    s.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
