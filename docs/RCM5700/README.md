# RCM5700 / Rabbit 5000 support in OpenRabbit

This directory documents the work done to make OpenRabbit talk to a
**Digi Rabbit RCM5700W** (Rabbit 5000) from Linux, and the custom
RAM-resident flash programmer built for it.

## TL;DR

| Capability | Status |
|---|---|
| Identify the RCM5700 (CPU + flash info) | **Works** |
| Run an SDCC program from the on-chip 128 KB SRAM | **Works** |
| Erase / program / verify the S29AL008D flash | **Works** |
| Build a real Dynamic C RCM5700 image (DC under Wine) | **Works** |
| Execute a program at flash offset 0 (flash boot) | **Not working** |

The flash programming is complete and verified, and Dynamic C 10 runs under
Wine and builds proper RCM5700 images ([dynamic-c-wine.md](dynamic-c-wine.md)).
**But flash boot does not happen**: a program at flash offset 0 is never
executed on reset, in either Program or Run Mode. A bare asm `ledblink` blinks
PD0 when loaded into RAM but not when flashed; the STATUS/DSR "it toggles"
signal was an artifact (a DC program that never touches STATUS toggles it too).
The open question is *why* offset 0 is not fetched - most likely the reset
memory mapping (`SYSCFG0` strapping) or the flash chip-select/inversion. See
[status-and-todo.md](status-and-todo.md) for the full analysis and the
flash-marker test that will settle it.

## Quick start

Build OpenRabbit as usual (`autoreconf -i && ./configure && make`). Then,
with the official FT232R USB programming cable on `/dev/ttyUSB0`:

```sh
# Identify the target (RCM5700 needs --ramcr 0x43: on-chip RAM, no external RAM)
./src/openrabbitfu --verbose --slow --ramcr 0x43 /path/to/any.bin /dev/ttyUSB0

# Run a program from the on-chip SRAM (no flashing)
./src/openrabbitfu --slow --ramcr 0x43 --ram --serialout \
    rcm5700/ramhello.bin /dev/ttyUSB0

# Erase, program and verify the flash, then reboot
./src/openrabbitfu --slow --ramcr 0x43 \
    --programmer rcm5700/rcmprog.bin \
    --serialout /path/to/target.bin /dev/ttyUSB0
```

`rcm5700/rcmprog.bin` is the prebuilt RAM flash programmer. Its source is
`rcm5700/rcmprog.c`; see [building.md](building.md) to rebuild it.

## Documents

* [hardware.md](hardware.md) — RCM5700/Rabbit 5000 hardware, flash part and memory map.
* [openrabbit-changes.md](openrabbit-changes.md) — the host-side changes (`--ram`, `--programmer`).
* [flash-protocol.md](flash-protocol.md) — the serial protocol used by the RAM programmer.
* [building.md](building.md) — building the helper programs with SDCC, incl. the crt0 patch.
* [dynamic-c-wine.md](dynamic-c-wine.md) — installing Dynamic C 10 under Wine and building real RCM5700 images.
* [investigation-log.md](investigation-log.md) — how we got here, with evidence.
* [status-and-todo.md](status-and-todo.md) — what works, what does not, next steps.
* [troubleshooting.md](troubleshooting.md) — common failure modes.

## Directory layout

```
rcm5700/                 helper programs and build scripts (see building.md)
  rcmprog.c/.bin         RAM-resident S29AL008D flash programmer (the deliverable)
  rcmflash.c             on-target self test (destructive: erases sector 0)
  flashid.c              reads the JEDEC flash ID
  flashscan.c            scans the flash for non-blank regions
  readtest.c             reads flash at several bank mappings
  memtest.c              probes which bank exposes the on-chip SRAM
  ramhello.c             minimal RAM-run test ("RCM5700 RAM OK")
  flashtest.c            flash-run test ("RCM5700 FLASH OK"); runs from RAM
  tiny.s/.bin            hand-written asm boot test (emits 'A')
  flashbeacon.s          minimal offset-0 beacon (emits 'X' at the reset clock)
  ledblink.s             bare offset-0 image: blinks PD0 (the real flash-boot test)
  statusbeacon.s         offset-0 test: drives STATUS high
  statusblink.s          offset-0 test: toggles STATUS
  bootbeacon.s           offset-0 image: toggles STATUS + emits serial
  boothello.s            offset-0 image: full DC-BIOS-style init, then prints "HI"
  statusflash.c          Dynamic C test: toggles STATUS (GOCR)
  dchello.c              Dynamic C test: prints over serial A (DC stdio)
  rcm5700.dcp            Dynamic C project (target config) for targetless builds
  statusread.py          reads STATUS (cable DSR) after a reset
  serialcap.py           DTR/RTS-aware serial capture helper
  build.sh               build a RAM program (patches crt0 STACKSEG)
  buildflash.sh          build a flash program (patches crt0 for RCM5700)
docs/RCM5700/            this documentation
```

## Important warning

The flash programmer is **destructive**. The self-test `rcmflash.c` and the
flashing flow erase flash sectors, and during development the first flash
sector of the test board was overwritten, so the board's original firmware no
longer boots. Note that at present **no** image at flash offset 0 boots (see
[status-and-todo.md](status-and-todo.md)); the board can still be re-flashed
because cold-boot mode (the SMODE bootstrap) lives in the CPU ROM.

## Handing this off

Start with [status-and-todo.md](status-and-todo.md): it has the current
working/not-working state, the Dynamic C 10 register reference, and the
concrete next steps for the flash-boot problem. Then
[investigation-log.md](investigation-log.md) for the full history and the
"dead ends worth remembering".
