# Status and TODO

## Works

* **Identification.** `--ramcr 0x43` connects to the RCM5700W and reports
  `CPU: 0x0300 (Rabbit 5000)` plus flash/RAM timing info.
* **RAM execution.** `--ram` loads an SDCC program into the 128 KB on-chip SRAM
  and runs it; serial output is visible with `--serialout`.
* **Flash programming.** `--programmer rcm5700/rcmprog.bin` erases, programs and
  fully verifies the S29AL008D. The JEDEC ID (`01 DA`), sector erase and byte
  program are all confirmed.

## Not working

* **Booting the flashed image.** After `rabbit_start` (reset + bootstrap-exit
  triplets) the flashed program does not run. The flash content and address
  mapping are verified correct; a 63-byte assembly program at flash offset 0
  produces no output. A cold-boot test with the programming cable attached is
  inconclusive because the cable appears to hold SMODE in bootstrap.

## Next steps (boot issue)

1. **Rabbit 5000 reset/bootstrap.** Re-read the Rabbit 5000 user manual's reset
   chapter (analogous to the Rabbit 4000 `3resboot.htm`) to confirm exactly
   where the boot ROM transfers control when SPCR bit 7 is set, and whether the
   RCM5700 uses top-boot address inversion (top sectors mapped to address 0).
2. **Try flashing at the top.** The S29AL008D is top-boot; try placing the test
   program in the top boot sector region and see if it boots.
3. **RAM trampoline.** Have the programmer copy a tiny routine to the on-chip
   SRAM, remap `MB0CR`/`MB1CR` to /CS0 and jump to 0 from RAM (the standard
   Rabbit flash-driver relocation trick). This avoids relying on
   `rabbit_start`.
4. **SMODE wiring.** Confirm whether the programming cable drives SMODE; if so,
   a plain reset will always enter bootstrap and `rabbit_start` is the only way
   out.
5. **Compare with Dynamic C.** Look at how Dynamic C's RFU starts a program from
   flash on the RCM5700 (`_PB_StartRegBiosFLASH` in `pilot.c` is the reference
   for other boards).

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
