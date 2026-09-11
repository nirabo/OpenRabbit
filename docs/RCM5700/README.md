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
| Execute a program at flash offset 0 (true Run Mode) | **Works** |
| Serial output from a flash boot | **Not working yet** |

The flash programming itself is complete and verified. Flash **boot** works too:
with **JP1 pins 1-2 removed** (Run Mode) and the board powered from **J6
(+5 V)**, a program at flash offset 0 executes (`rcm5700/bootbeacon.s` toggles
STATUS). Two things defeated earlier attempts: RTS does **not** control SMODE
(JP1 does), and USB power reboots the board ~2 s after startup in Run Mode.
The open issue is now getting **serial output** from a flash boot and running
full SDCC programs (which need a DC-BIOS-style preamble). See
[status-and-todo.md](status-and-todo.md).

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
  statusbeacon.s         offset-0 test: drives STATUS high if it runs
  statusblink.s          offset-0 test: toggles STATUS slowly if it runs
  bootbeacon.s           offset-0 image: toggles STATUS + emits serial (proves flash boot)
  boothello.s            offset-0 image: full DC-BIOS-style init, then prints "HI"
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
