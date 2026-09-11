#!/bin/bash
# Build an RCM5700 RAM program with SDCC and patch the crt0 STACKSEG value so
# the stack/data live in the on-chip SRAM (physical 0x18000) instead of 0x76000.
# See docs/RCM5700/building.md.
set -e
name="$1"
here="$(cd "$(dirname "$0")" && pwd)"
EX="$here/../examples"
cd "$here"
sdcc -mr2k -I"$EX" --data-loc 0xa000 -DRCM5700 -c "$name.c" -o "$name.rel"
sdcc -mr2k -I"$EX" --data-loc 0xa000 "$name.rel" -o "$name.ihx"
objcopy -I ihex -O binary "$name.ihx" "$name.bin"
python3 - "$name.bin" <<'PY'
import sys
p=sys.argv[1]
d=bytearray(open(p,'rb').read())
assert d[0x10]==0x3e and d[0x11]==0x76 and d[0x12]==0xd3 and d[0x13]==0x32 and d[0x14]==0x11, d[0x10:0x16].hex()
d[0x11]=0x18
open(p,'wb').write(d)
print("patched STACKSEG 0x76->0x18 in", p)
PY
