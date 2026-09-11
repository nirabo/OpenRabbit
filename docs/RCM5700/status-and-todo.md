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
  a program with a valid RCM5700 stack (on-chip SRAM via `MB2CR=0x43`) that
  prints correctly when loaded with `--ram`.

## Fixed during bring-up

* **Serial framing.** `tty_setbaud()` used `CSTOPB` (2 stop bits). The Rabbit
  bootstrap needs 8N1; with 2 stop bits the coldload triplets were frequently
  rejected. Removed.
* **SMODE / RTS.** The programming cable drives SMODE from RTS (RTS low =
  Program Mode / bootstrap, RTS high = Run Mode). OpenRabbit now sets this
  (`rabbit_smode()`), though it is only reliable with the JP1 jumper in place.
* **`--baud <n>`** added for `--serialout` diagnostics.

## Not working

* **Booting the flashed image.** Neither a `rabbit_start` reset nor a real
  Run Mode reset (JP1 removed, power-cycle) runs the flashed program. Reading
  logical `0x0000` with `MB0CR=/CS0` returns `0xFF`, even though the programmer
  reads the same flash (offset 0) correctly through the data-segment window at
  physical `0x100000`. So the reset-time mapping of the parallel flash to
  logical `0x0000` is not what a naive `MB0CR=/CS0` gives.

## Next steps (boot issue)

1. **Replicate the Dynamic C BIOS MMU setup.** `DCRabbit_10`'s
   `Lib/Rabbit4000/BIOSLIB/StdBios.c` `dkSetMMU` + `MB0CR_SETTING` /
   `MECR_VALUE (0x20)` / `FLASH_WSTATES` (from the ID block `flashMBC`) is the
   authoritative sequence. In particular check `MB0CR_INVRT_A18/A19` and
   `MACR` (8- vs 16-bit) for the RCM5700.
2. **Read the flash through the same window the programmer uses.** Confirm
   whether physical `0x000000` and physical `0x100000` really alias (i.e. the
   flash decodes only `A[19:0]`), or whether the board gates `/CS0` on a bank
   select bit.
3. **RAM trampoline.** `rcm5700/flashboot.s` remaps `MB0CR` from the stack
   segment and jumps to 0; the remap code runs (verified with serial markers)
   but the flash read at 0 still returns `0xFF`. Finish once (1)/(2) are known.
4. **ID block.** Inspect the RCM5700 System ID block (`flashMBC`, memory
   config); a boot ROM may consult it before/independently of the reset fetch.
5. **JP1.** Programming reliably needs the JP1 1-2 jumper installed (Program
   Mode); RTS alone is not dependable on this Interface Board.

## Cleanups / improvements

* Replace the binary `STACKSEG`/`MBxCR` patching with a proper RCM5700
  `crt0`/linker configuration.
* Auto-detect the RCM5700 instead of requiring explicit `--ramcr 0x43` and
  `--programmer`.
* Embed the programmer binary in `openrabbitfu` (like the built-in
  coldload/pilot) so no external file is needed.
* Program in larger chunks and/or word-wise; the current byte-at-a-time JEDEC
  programming is slow.
* Add an option to read/dump the flash (the programmer already supports `'R'`).
* Fix the ID block parsing for the v5 layout (the `idBlock2` fields).

## Known-good workaround

For development that does not need persistence, use `--ram` to run programs
directly from the on-chip SRAM. For persistent images until the boot issue is
solved, use Dynamic C 10 / the Digi RFU (e.g. under Wine).
