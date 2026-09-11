# Dynamic C 10 under Wine (and building RCM5700 images)

This documents a working Dynamic C 10.72E install under Wine, and the
**targetless** build recipe that produces a proper RCM5700 image (BIOS +
program) without a board attached. This is how we obtain a real Dynamic C BIOS
image to compare against / flash.

## Install (silent NSIS)

The installers are Nullsoft (NSIS), so they install silently:

```sh
wineboot -u
cd /path/to/dynamic-c-downloads
wine DynamicC_10.72E.exe /S /D=C:\\DCRABBIT_10
```

Installs to `~/.wine/drive_c/DCRABBIT_10`. Key executables:

* `Dccl_cmp.exe` — command-line compiler (works under Wine).
* `Dcwd.exe` — GUI IDE.
* `Utilities/Rfu.exe` — Rabbit Field Utility (Digi's flasher).

Wine auto-maps the USB programming cable: `/dev/ttyUSB0` -> **COM33**
(see `~/.wine/dosdevices/`).

## Targetless build recipe (the important part)

Dynamic C needs a target configuration. For the CLI, put it in a `.dcp`
project file's `[Targetless Compile]` section (this is exactly what the IDE's
*Compile -> Compile to .bin file -> Define target configuration* dialog
writes). A ready-to-use file is in `rcm5700/rcm5700.dcp`:

```ini
[Targetless Compile]
Board ID=0x3100
CPU ID=5000
CPU Revision=0
Crystal Speed MHz=25.0000
RAM Size KBytes=128
Flash Size KBytes=1024
Description=RCM5700, 50MHz, 128K SRAM, 1M Flash
Macros=CLK_DBL=1;BRD_OPT0=0x20;MD0=0x1;MD0_ID=0x1DA;...;MD2_MBC=0xC3
[Include BIOS]
Include BIOS=1
```

The `Macros=` string is copied from `DCRabbit_10/TCData.ini`'s `[RCM5700]`
entry. Then:

```sh
wine ~/.wine/drive_c/DCRABBIT_10/Dccl_cmp.exe \
     'Z:\path\to\prog.c' \
     -pf 'Z:\path\to\rcm5700.dcp' \
     -br -rb+ -mf
```

* `-br` compile to `.bin` using rti parameters (targetless)
* `-rb+` include the BIOS
* `-mf`  BIOS memory setting = flash

Output `prog.bin` (~68-82 KB) starts with the real BIOS entry:

```
3e 00        ld a,0x00
d3 32 1d 00  ioi ld (0x1d),a   ; MACR=0
00 00        nop nop
3e 80 ...    MMIDR=0x80
3e c0 ...    EDMR=0xC0
c3 84 00     jp 0x0084
```

and contains the string `DynamicC Universal Rabbit BIOS Version 10.70`.

These images **flash and verify** correctly with OpenRabbit
(`--programmer rcm5700/rcmprog.bin prog.bin`), but they do **not** run from a
cold flash boot (see [status-and-todo.md](status-and-todo.md)).

## Attached-target build (needs Program Mode)

`-b` builds using the attached target instead of an RTI file:

```sh
wine ~/.wine/drive_c/DCRABBIT_10/Dccl_cmp.exe 'Z:\path\to\prog.c' \
     -b -rb+ -mf -s 33:115200:1:0
```

With JP1 installed (Program Mode) DC's cold loader reached the board, but the
**Pilot BIOS transfer failed** (`An internal error in the target communication
connection has occurred`). With JP1 removed it reports `No Rabbit Processor
Detected`. So the targetless recipe above is the reliable path.

## RTI file format (unresolved)

`-rti '<string>'` and `-rf <file>` take a target config, but the exact syntax
was not cracked. The IDE labels are `Board ID`, `CPU ID`, `CPU Revision`,
`Crystal Speed MHz`, `RAM Size KBytes`, `Flash Size (KBytes)`; the internal
field names are `boardType`, `cpuId`, `crystalSpeed`, `ramSize`, `flashSize`.
`-rti` kept failing with `RTI PARAMETERS ERROR: crystalSpeed`. Use the `.dcp`
`[Targetless Compile]` route instead.

## Samples that print / blink

* `Samples/RCM5700/FLASHLED01.C` blinks **PD0** (DS1 LED).
* A minimal DC stdio test is in `rcm5700/dchello.c` (`printf` over serial A).
* `rcm5700/statusflash.c` toggles GOCR (STATUS) — used as a probe.
