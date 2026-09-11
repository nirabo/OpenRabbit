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

## Next steps (boot issue)

1. **Check `SYSCFG0`/`SYSCFG1` strapping** on the RCM5700 module schematic. If
   `SYSCFG0` is high, the CPU starts from internal SRAM and a bare program at
   flash offset 0 can never boot; the program must be loaded into SRAM (as the
   coldloader does) or the flash mapping must be set up by a boot loader.
2. **Rule out the cable/JP1 holding SMODE high** during the Run Mode test: use
   the AC adapter and a separate serial connection, or an LED indicator, so the
   programming cable cannot influence SMODE.
3. **Compare against a known-good Dynamic C image.** Flash a Dynamic C-built
   image (e.g. under Wine) and confirm it boots; then diff its first bytes and
   MMU setup against ours.
4. **Verify the ID block** at the top of flash (marker `55 AA 55 AA ...`).
   `rcm5700/rcmflash.c` read `4A 00 4A 00 4A 00` at `0xFFFFA`, which is not the
   documented marker, so the top of flash may not hold a valid ID block.

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
