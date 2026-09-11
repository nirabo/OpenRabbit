#!/bin/bash
# Build a program intended to run FROM FLASH on the RCM5700. Patches the SDCC
# crt0 to map the stack/data to the on-chip RAM:
#   MB2CR=0x05 -> MB1CR=0x43 (/CS3 on-chip RAM)
#   STACKSEG 0x76 -> 0x40 (physical 0x040000, on-chip RAM via MB1)
# See docs/RCM5700/building.md. NOTE: booting flashed programs is not yet solved.
set -e
name="$1"
here="$(cd "$(dirname "$0")" && pwd)"
EX="$here/../examples"
cd "$here"
sdcc -mr2k -I"$EX" --data-loc 0xa000 -DRCM5700 -c "$name.c" -o "$name.rel"
sdcc -mr2k -I"$EX" --data-loc 0xa000 "$name.rel" -o "$name.ihx"
objcopy -I ihex -O binary "$name.ihx" "$name-flash.bin"
python3 - "$name-flash.bin" <<'PY'
import sys
p=sys.argv[1]
d=bytearray(open(p,'rb').read())
assert d[0x04]==0x3e and d[0x05]==0x05 and d[0x06]==0xd3 and d[0x07]==0x32 and d[0x08]==0x16, d[0x04:0x0a].hex()
d[0x05]=0x43; d[0x08]=0x15          # MB1CR = 0x43 (/CS3)
assert d[0x10]==0x3e and d[0x11]==0x76, d[0x10:0x13].hex()
d[0x11]=0x40                        # STACKSEG = 0x40
open(p,'wb').write(d)
print("flash-patched", p)
PY
