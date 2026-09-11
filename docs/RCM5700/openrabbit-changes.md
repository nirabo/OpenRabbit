# OpenRabbit host changes

All changes are additive; existing Rabbit 2000/3000/4000 behaviour is
unchanged. `--ram` and `--programmer` are only accepted in RFU mode
(`openrabbitfu`).

## `src/openrabbit.c`

* New global `unsigned int ramrun;`.
* New option `--ram`: load the target into RAM (physical 0) and start it there
  instead of flashing. Implies `--run`. Only valid for `openrabbitfu`.
* New option `--programmer <file>`: load the RAM-resident flash programmer
  `<file>`, run it, stream the target image into flash, verify and reboot.
  Implies `--ram`.
* The file passed to `rabbit_program()` is `programmerfile` when `--programmer`
  is used, otherwise the positional target file.
* The serial-output ("STATUS") wait loop now has a 0.5 s timeout instead of
  blocking forever.

Programmer flow (after the usual coldload + pilot + RAM upload):

```c
if (programmerfile) {
    puts("Starting RAM programmer.");
    rabbit_start_ram(tty);
    tty_setbaud(tty, 38400);
    rcm5700_flash(tty, argv[1]);   /* argv[1] is the target file */
    puts("Rebooting and running flashed code.");
    rabbit_start(tty);
    /* optional --serialout */
}
```

## `src/rabbit.c`

* `rabbit_upload()` honours the new `ramrun` global:
  * the write target is physical `0x0` instead of `WP_DATA_ORG` (0x80000);
  * the flash-data and erase-flash packets are skipped (they are meaningless
    when loading to RAM).
* New `rabbit_start_ram()` — sends `TC_SYSTEM_STARTBIOS` with
  `TC_STARTBIOS_RAM`, which makes the still-running pilot jump to a program
  previously written into RAM at physical 0. The pilot does not acknowledge
  this request (it jumps away immediately).
* New `serial_read_timeout()` — select()-based read with a timeout.
* New `rcm5700_flash()` — drives the RAM programmer's serial protocol (see
  [flash-protocol.md](flash-protocol.md)): sync, erase, 128-byte program
  chunks, and a full read-back verify.
* New `rabbit_smode()` — drives SMODE from the cable's RTS line (RTS low =
  Program Mode / bootstrap, RTS high = Run Mode). Called from `rabbit_open()`
  (Program Mode) and `rabbit_start()` (Run Mode). Boards that strap SMODE high
  ignore it.

## `src/myio.c`

* `tty_setbaud()` no longer sets `CSTOPB` (2 stop bits). The Rabbit bootstrap
  needs 8N1; with 2 stop bits the coldload triplets were frequently rejected.
  The termios struct is also zero-initialised.

## `src/openrabbit.c`

* New option `--baud <n>` selects the `--serialout` baud rate (default 38400).

## `src/rabbit.h`

* Declarations for `rabbit_start_ram()` and `rcm5700_flash()`, and
  `extern unsigned int ramrun;`.

## Usage summary

```
openrabbitfu [--verbose] [--slow] [--ramcr <i>] [--coldload <f>] [--pilot <f>]
             [--ram] [--programmer <f>] <target.bin|target.ihx> <device>
```

For an RCM5700 you must pass `--ramcr 0x43` (see
[hardware.md](hardware.md) — the on-chip SRAM is on /CS3, and there is no
external RAM on /CS1).

## Notes / limitations of the current host code

* The programmer baud is fixed at 38400 and the host switches to it after
  starting the RAM programmer.
* `rcm5700_flash()` assumes the target is programmed starting at flash offset
  0. `WP_DATA_ORG`/`0x80000` is not used on this path.
* The `--programmer` path does not currently auto-detect the RCM5700; it must be
  selected explicitly.
