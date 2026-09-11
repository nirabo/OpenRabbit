# Status and TODO

## Works

* **Identification.** `--ramcr 0x43` connects to the RCM5700W and reports
  `CPU: 0x0300 (Rabbit 5000)` plus flash/RAM timing info.
* **RAM execution.** `--ram` loads an SDCC program into the 128 KB on-chip SRAM
  and runs it; serial output is visible with `--serialout`. Confirmed with
  `ramhello.bin` (`RCM5700 RAM OK`) and with `ledblink.bin` blinking the DS1 LED
  on PD0.
* **Flash programming.** `--programmer rcm5700/rcmprog.bin` erases, programs and
  fully verifies the S29AL008D. JEDEC ID (`01 DA`), sector erase, byte program
  and read-back all confirmed.
* **Building real Dynamic C images.** Dynamic C 10.72E is installed under Wine
  and a **targetless** build produces a proper RCM5700 image with the real
  Dynamic C BIOS. See [dynamic-c-wine.md](dynamic-c-wine.md). DC-built images
  (`FLASHLED01.bin`, `dchello.bin`) flash and verify correctly.

## Not working (the core problem)

* **Flash boot does not happen.** A program at flash offset 0 is **not**
  executed on reset, in Program Mode or Run Mode:
  * `rcm5700/ledblink.s` (bare asm at offset 0) blinks PD0 when loaded into RAM
    by the pilot, but does **not** blink when flashed and the board is reset.
  * DC's `FLASHLED01.bin` (with the real BIOS) does not blink PD0 from flash.
  * `rcm5700/statusflash.c` (DC) / `bootbeacon.s` (asm) appeared to toggle
    STATUS, but that is an **artifact**: `dchello.c` (which never touches
    STATUS/GOCR) toggles STATUS identically in Run Mode. Do **not** trust the
    STATUS/DSR line as a "program ran" indicator in Run Mode.
  * Serial output from a flash boot was never observed either (but this is
    moot until offset 0 executes at all).

## What was tried for flash boot

* **JP1.** Program Mode = JP1 pins 1-2 jumpered; Run Mode = jumper removed
  (RCM5700/RCM6700 User's Manual 90001191). Confirmed: with JP1 installed the
  board bootstraps (coldloader/pilot work); with it removed the board does not
  respond to bootstrap triplets.
* **Power.** J6 is the DC input jack (**+5 V DC**; module runs on 3.3 V from the
  on-board regulator). USB power makes the board reboot ~2 s after startup in
  Run Mode, so J6 was used.
* **Reset vs power-cycle.** Both the RESET button and a full J6 power-cycle
  (JP1 removed) fail to run the flashed image.
* **RTS/SMODE.** RTS does **not** control SMODE on this board; JP1 does.

## Fixed during bring-up

* **Serial framing.** `tty_setbaud()` used `CSTOPB` (2 stop bits). The Rabbit
  bootstrap needs 8N1; removed. (`src/myio.c`)
* **SMODE / RTS.** `rabbit_smode()` added, but unreliable without JP1.
* **`--baud <n>`** added for `--serialout`.
* **`buildflash.sh`** patches the SDCC crt0 `MB2CR 0x05 -> 0x43`.

## Next steps

The single open question is **"does the CPU execute flash offset 0 at all?"**.
The evidence says no. Candidates, in rough order of likelihood:

1. **Reset mapping / `SYSCFG0`.** On the Rabbit 5000, `SYSCFG0` low => reset MB0
   = `/CS0` (parallel flash, 8-bit, 4 wait); `SYSCFG0` high => MB0 = `/CS3`
   internal SRAM. If the RCM5700 module straps `SYSCFG0` high, a bare program at
   flash offset 0 can never boot. **Get the RCM5700 module schematic / MiniCore
   datasheet and check the `SYSCFG0`/`SYSCFG1` straps.**
2. **Flash chip select / address inversion.** Confirm the flash is really on
   `/CS0` with no A18/A19 inversion. Dynamic C's `BOARDTYPES.LIB` says
   `CS_FLASH=CS0OE0`, `MB0CR_INVRT=0`, 8-bit (`BRD_OPT0=0x20`), but the board
   could differ. Cross-check with the module schematic.
3. **Reset vector location.** The S29AL008D is a *top-boot* device. Confirm the
   CPU reset vector is at physical 0 and not at the top of the flash (or that
   the flash isn't remapped). The original factory firmware occupied
   `0x00000-0x06FFF` and `0xF0000-0xFFFFF`; try flashing a blink image at the
   **top** of the flash too (requires a programmer tweak to write at an offset).
4. **Definitive cable-independent test.** Have the offset-0 image erase and
   program a marker into a spare flash sector (e.g. `0x80000`, blank per the
   scan) and read it back with the programmer. A marker proves execution; a
   blank sector proves the CPU never fetched offset 0. This removes all
   dependency on STATUS/serial/LED.
5. **Compare against a known-good image.** Flash a DC-built image with DC's own
   RFU (`Utilities/Rfu.exe`) instead of OpenRabbit, in case the raw programming
   order/ID block matters.
6. **ID block.** `rcm5700/rcmflash.c` read `4A 00 4A 00 4A 00` at `0xFFFFA`
   instead of the documented `55 AA 55 AA 55 AA`. The CPU does not read the ID
   block to boot, but DC/RFU do; writing a valid ID block (`Utilities/Write_ID`)
   is worth trying.
7. **Try SDCC 4.6.0's Rabbit 5000 port.** SDCC 4.6.0 (2026-06) added an
   experimental `r5k` port and `__far` 1 MB address-space support, which may
   handle the RCM5700 memory map more directly than the `r2k` port we used. See
   [future-and-ecosystem.md](future-and-ecosystem.md).

If offset 0 *does* execute but the LED/serial simply don't show it, the
flash-marker test (item 4) is the way to prove it.

## What the Dynamic C 10 sources establish (reference)

* For a parallel-flash RCM5700 the BIOS root is at **physical flash offset 0**
  (`Lib/Rabbit4000/memory_layout.lib:311-330`, `ORG_FLASH_START 0x0`) and the
  first code there is `_biosentry_` (`StdBios.c:1556`): `MACR=0x00` + 2 nops,
  `MMIDR=0x80`, `EDMR=0xC0`.
* **Nothing writes `MB0CR` before the first fetch.** The BIOS assumes reset
  already maps `/CS0` into bank 0 (via `SYSCFG0`). `SYSCFG` appears nowhere in
  the DC10 tree - it is hardware only.
* `dkSetMMU` (`StdBios.c:1707`) sets `MECR=0x20`, `SEGSIZE=0xD6`,
  `DATASEGL/H=0x0100`, `MB0CR=0x00`, `MB1CR=0x00`, `MB2CR=0xC3`, `MB3CR=0x00`.
* The only code that explicitly re-maps bank 0 to flash and jumps to 0 is the
  pilot, after a host bootstrap (`ColdBoot/PILOT.C:650-668`).
* DC10 cannot be ported to Linux (closed-source Windows compiler,
  Z-World-specific library sources) - but it **runs under Wine**, which is what
  we use.

## Cleanups / improvements

* Embed the programmer binary in `openrabbitfu`.
* Auto-detect the RCM5700 instead of requiring `--ramcr 0x43` / `--programmer`.
* Add a flash read/dump option (the programmer already supports `'R'`).
* Add a "flash at offset" option to enable the top-of-flash test (item 3).

## Known-good workaround

Use `--ram` to run programs from the on-chip SRAM (works reliably). For
persistent images, Dynamic C 10 / the Digi RFU can be run under Wine.
