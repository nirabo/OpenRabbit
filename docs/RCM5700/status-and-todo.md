# Status and TODO

## Works

* **Identification.** `--ramcr 0x43` connects to the RCM5700W and reports
  `CPU: 0x0300 (Rabbit 5000)` plus flash/RAM timing info.
* **RAM execution.** `--ram` loads an SDCC program into the 128 KB on-chip SRAM
  and runs it; serial output is visible with `--serialout`.
* **Flash programming.** `--programmer rcm5700/rcmprog.bin` erases, programs and
  fully verifies the S29AL008D. The JEDEC ID (`01 DA`), sector erase and byte
  program are all confirmed.
* **Flash programs that run from RAM.** `buildflash.sh` + `flashtest.c` produce
  a program with a valid RCM5700 stack (on-chip SRAM via `MB2CR`) that prints
  correctly when loaded with `--ram`.

## Fixed during bring-up

* **Serial framing.** `tty_setbaud()` used `CSTOPB` (2 stop bits). The Rabbit
  bootstrap needs 8N1; with 2 stop bits the coldload triplets were frequently
  rejected. Removed.
* **SMODE / RTS.** The programming cable can drive SMODE from RTS. OpenRabbit
  now sets it (`rabbit_smode()`), though it proved unreliable without the JP1
  jumper.
* **`--baud <n>`** added for `--serialout` diagnostics.

## Not working

* **Booting the flashed image.** Neither a `rabbit_start` reset nor a real
  Run Mode reset (JP1 removed, power-cycle) runs a program placed at flash
  offset 0. A 59-byte "beacon" that emits `X` at the reset clock
  (`rcm5700/flashbeacon.s`) produced **0 bytes at every baud (300-115200) and
  both RTS states**, so the CPU is not executing flash offset 0 at all.

### Reproducing the flash-boot test

```sh
# Build the beacon
cd rcm5700
sdasrab -o flashbeacon.rel flashbeacon.s
sdcc -mr2k --no-std-crt0 flashbeacon.rel -o flashbeacon.ihx
objcopy -I ihex -O binary flashbeacon.ihx flashbeacon.bin
cd ..

# Flash it (JP1 1-2 installed = Program Mode)
./src/openrabbitfu --verbose --slow --ramcr 0x43 \
    --programmer rcm5700/rcmprog.bin rcm5700/flashbeacon.bin /dev/ttyUSB0

# Remove JP1 1-2, power-cycle, then capture (expect 'X' if flash boot works)
python3 rcm5700/serialcap.py -b 2400 -t 5 /dev/ttyUSB0
```

`serialcap.py` drives DTR/RTS itself so it does not hold the target in reset.

### Baud-independent STATUS test

```sh
# Build the STATUS beacon/blink (small hand-written asm at flash offset 0)
cd rcm5700
for f in statusbeacon statusblink; do
    sdasrab -o $f.rel $f.s
    sdcc -mr2k --no-std-crt0 $f.rel -o $f.ihx
    objcopy -I ihex -O binary $f.ihx $f.bin
done
cd ..

# Flash one of them, then read STATUS (cable DSR) after a reset
./src/openrabbitfu --slow --ramcr 0x43 \
    --programmer rcm5700/rcmprog.bin rcm5700/statusblink.bin /dev/ttyUSB0
python3 rcm5700/statusread.py --rts run --repeat 12 /dev/ttyUSB0
```

`statusbeacon.s` drives STATUS high and holds it; `statusblink.s` toggles it
slowly. If the CPU executes flash offset 0 the DSR line must follow; in our
tests it did not, and the target still responded to GOCR bootstrap triplets
(see investigation-log section 10).

## What the Dynamic C 10 sources say (reference)

Reference tree: `https://github.com/digidotcom/DCRabbit_10` (a local clone was
used at `~/projects/sandbox/DCRabbit_10`). From it:

* On reset with **`SYSCFG0` low**, `PC=0` already fetches from `/CS0` flash
  (8-bit, 4 wait) — no MMU write is needed to *begin* executing at offset 0.
  If `SYSCFG0` is strapped **high**, reset MB0 is `/CS3` (internal SRAM,
  16-bit) and the CPU does not fetch from flash.
* The RCM5700 BIOS entry (`Lib/Rabbit4000/BIOSLIB/StdBios.c:1556`
  `_biosentry_`) sets `MACR=0x00`, `MMIDR=0x80`, `EDMR=0xC0`.
* `dkSetMMU` (`StdBios.c:1707`) sets `MECR=0x20`; the bank values are
  `MB0CR=0x00` (`/CS0` flash, 4 wait), `MB1CR=0x00`, `MB2CR=0xC3`
  (`/CS3` on-chip SRAM, 0 wait), `MB3CR=0x00`; `SEGSIZE=0xD6`,
  `DATASEGL/H=0x0100` (data segment -> physical `0x100000`).
* No address-line inversion (`MB0CR_INVRT_A18/A19` are 0 for `RCM5700_SERIES`);
  the parallel flash is 8-bit (`BRD_OPT0=0x20`, `_ENABLE_16BIT_FLASH_` off);
  ID block `flashMBC=0x00`, `ramMBC=0xC3`.

## Why not just port Dynamic C 10 to Linux?

This came up as a shortcut. It is not viable:

* `DCRabbit_10` is the **library/source tree**, not a toolchain. The compiler
  (`dccl_cmp.exe`) is closed-source and Windows-only; the `ColdBoot/Makefile`
  invokes it directly.
* `Lib/Rabbit4000/*.LIB` and `ColdBoot/*.C` use Z-World compiler extensions
  (`#asm`, `root`/`xmem`, `far`, `_cexpr`, `//@` triplet directives) that
  SDCC/GCC cannot compile.
* The `Bios/*.bin` files are Rabbit **target** binaries, not host tools; they
  do not need "Linux compatibility". OpenRabbit already ships its own
  equivalents (`coldboot/coldload.bin`, `coldboot/pilot.bin`).

What DC10 *does* give us is the authoritative **boot contract** and the
**detection technique**, both captured above and in
[investigation-log.md](investigation-log.md). Use those, not the compiler.

## Next steps (boot issue)

The decisive question is no longer "is the image correct" but **"does the CPU
execute flash offset 0 at all?"**. Evidence so far says it does not, and the
most likely reason is that the board is still in **bootstrap mode** during our
tests (see investigation-log section 10): with RTS asserted ("Run Mode") the
target still executed GOCR bootstrap triplets, and a STATUS beacon/blink placed
at offset 0 was never observed.

1. **Get the module into true Run Mode.** JP1 (or whatever straps SMODE0/1)
   must select Run Mode, and the programming cable must not hold SMODE. Test
   with the AC adapter and a *separate* serial connection so the cable cannot
   influence the SMODE/STATUS lines. Confirm with
   `python3 rcm5700/statusread.py --rts run`: after a Run-Mode reset the
   `statusbeacon.s`/`statusblink.s` image should drive STATUS; today it does
   not.
2. **Check `SYSCFG0`/`SYSCFG1` strapping** on the RCM5700 module schematic. If
   `SYSCFG0` is high, reset MB0 = `/CS3` internal SRAM and a bare program at
   flash offset 0 can never boot.
3. **Definitive test independent of serial/STATUS:** have the offset-0 image
   erase and program a marker into a spare flash sector (e.g. `0x80000`, which
   the scan showed blank), then read it back with the programmer. A marker
   proves execution; a blank sector proves the CPU never fetched offset 0.
4. **Once offset 0 is confirmed, flash a complete image**, i.e. a DC10-style
   boot preamble (`MACR`/`MMIDR`/`EDMR`, `MECR`/`SEGSIZE`/`DATASEG`, `MB0..3CR`,
   clock, stack) followed by the program. A bare SDCC program does not include
   the BIOS that normally does this.
5. **Verify the ID block** at the top of flash (marker `55 AA 55 AA ...`).
   `rcm5700/rcmflash.c` read `4A 00 4A 00 4A 00` at `0xFFFFA`, which is not the
   documented marker, so the top of flash may not hold a valid ID block. This
   does not affect the hardware boot (the CPU does not read it) but breaks
   Dynamic C / RFU identification.

## Cleanups / improvements

* Replace the binary `STACKSEG`/`MBxCR` patching with a proper RCM5700
  `crt0`/linker configuration that matches the BIOS's `MECR`/`SEGSIZE`/
  `DATASEG` values.
* Auto-detect the RCM5700 instead of requiring explicit `--ramcr 0x43` and
  `--programmer`.
* Embed the programmer binary in `openrabbitfu` (like the built-in
  coldload/pilot) so no external file is needed.
* Program in larger chunks and/or word-wise; the current byte-at-a-time JEDEC
  programming is slow.
* Add an option to read/dump the flash (the programmer already supports `'R'`).

## Known-good workaround

For development that does not need persistence, use `--ram` to run programs
directly from the on-chip SRAM. For persistent images, use Dynamic C 10 / the
Digi RFU (e.g. under Wine).
