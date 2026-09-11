# Investigation log

Chronological record of how RCM5700 support was developed, including dead ends
and evidence. Useful if this is ever picked up again.

## 1. Build and first contact

`autoreconf -i && ./configure && make` produced `src/openrabbit` /
`src/openrabbitfu` (built-in loaders; SDCC present). The programming cable is an
FT232R (`0403:6001`) at `/dev/ttyUSB0`.

First run failed with the FTDI falling off the USB bus (`error -71`, "unable to
enumerate") — the known FT232R/hub issue. After a re-plug and `chmod 666
/dev/ttyUSB0`, the initial loader loaded but the tool hung:

```
Secondary loader format detected as Dynamic C 9
<hang>
```

`strace` showed it blocked in `rabbit_pilot()` waiting for the coldload checksum
reply (`src/rabbit.c`).

## 2. Misidentified as a Rabbit 4000

The label "D40001038" was assumed to be a Rabbit 4000. A `--ramcr` sweep
(0x45, 0xc5, 0x05, 0x85 hang; 0xc8, 0x00, 0x48, 0x08 fail the status check) and
MACR (0x1D) prepend/append experiments (9 variants each, via a custom
`--coldload`) all failed. This matched upstream issue #36 (16-bit RCM40x0), but
was actually the wrong track.

## 3. Correct board: RCM5700W (Rabbit 5000)

With the real board known, `--ramcr 0x43` (on-chip RAM on /CS3) got much
further:

```
CPU: 0x0300 (Rabbit 5000)
Freq: 25000000
sectorSize: 0x0080  numSectors: 0x0013  flashSize: 0x0080
Flash speed: 70 ns  RAM speed: 15 ns
```

The subsequent flash write failed with `NAK for subtype 0x43`
(`TC_NAK | TC_SYSTEM_WRITE`). Root cause: the precompiled Dynamic C 9
`pilot.bin` flash driver (`_InitFlashDriver`/`FSM_XFlash`) does not know the
RCM5700's **S29AL008D** and is hardcoded for external RAM (`RAM_CS_TO_USE 0x45`).
It cannot be rebuilt without Dynamic C.

## 4. Running our own code from on-chip SRAM

Because the pilot can (a) write to RAM for physical addresses < 0x80000 and
(b) start a RAM program (`TC_STARTBIOS_RAM` → `0x0000`), we added `--ram` to
OpenRabbit and wrote a test program.

First attempt produced no output. Cause: SDCC's crt0 sets `STACKSEG = 0x76`
(physical 0x76000), which does not exist on the RCM5700. Patching the linked
binary's `STACKSEG` to `0x18` (physical 0x18000, on-chip SRAM) made it work:

```
RCM5700 RAM OK
RCM5700 RAM OK
...
```

## 5. Driving the flash

Research established the part (S29AL008D, top-boot), the memory map (flash on
/CS0, on-chip SRAM on /CS3, `MECR = 0x20`) and the JEDEC sequences (reference:
Dynamic C 10 `FLASHWR.LIB`).

A RAM program maps the flash into bank 2 (`MB2CR = 0x00`, `MECR = 0x20`) and
reaches it through the 8 KB data-segment window. The classic `DATASEG` register
is only 8 bits, so `DATASEGL`/`DATASEGH` must be used. The JEDEC ID then reads
correctly:

```
ID: mfg=01 dev=DA   (Spansion S29AL008D)
```

The on-target self test (`rcmflash.c`) then erased sector 0, programmed
`5A A5 00 FF` and read it back:

```
after erase @0: FF FF FF FF FF FF FF FF
after program @0: 5A A5 00 FF   (expect 5A A5 00 FF)
```

## 6. Streaming programmer and host integration

`rcmprog.c` wraps the flash routines in a small serial protocol
([flash-protocol.md](flash-protocol.md)). The host side is
`rcm5700_flash()` + the `--programmer` option. A full run:

```
$ ./src/openrabbitfu --verbose --slow --ramcr 0x43 \
      --programmer rcm5700/rcmprog.bin examples/hello/hello-RCM5700.bin /dev/ttyUSB0
...
Starting RAM programmer.
loaded 1166 bytes from examples/hello/hello-RCM5700.bin
Erasing 1166 bytes...
flashing... 100%
Flash verified OK (1166 bytes).
Rebooting and running flashed code.
```

## 7. Open issue: booting the flashed image

After flashing, the target never emits output — not even a 63-byte hand-written
assembly program (`tiny.s`) that only writes `A` to serial A. Verification
confirms the bytes are in flash at offset 0:

```
MB2 phys0x100000 (off 0x00000): 3E 51 D3 32 09 00 3E 54 D3 32 09 00 ...
```

(the start of `tiny`), and the bank addressing is self-consistent (physical
0x080000 reads blank = flash offset 0x80000). A cold boot with the programming
cable attached is inconclusive because the cable holds SMODE in bootstrap.

A flash scan of the test board showed:

```
0x00000-0x6FFFF   firmware
0x70000-0xEFFFF   blank
0xF8000-0xFFFFF   user + ID block
```

So the flash content and mapping look right; the remaining problem is how the
Rabbit 5000 leaves bootstrap / boots from flash. See
[status-and-todo.md](status-and-todo.md).

## 8. Fixes found later (framing, SMODE)

While trying to make the flashed image run, three host-side problems surfaced:

* **`tty_setbaud()` set `CSTOPB` (2 stop bits).** The Rabbit bootstrap is 8N1;
  with 2 stop bits the coldload triplets were frequently rejected. After
  removing it, programming became reliable. (`src/myio.c`)
* **SMODE was never driven.** The programming cable can drive SMODE from RTS
  (RTS low = Program Mode, RTS high = Run Mode). Added `rabbit_smode()` and
  called it from `rabbit_open()` / `rabbit_start()`. On this Interface Board it
  only worked reliably with the JP1 1-2 jumper installed. (`src/rabbit.c`)
* **`buildflash.sh` was wrong.** The stock crt0 maps the stack to external RAM
  on `/CS1`; for the RCM5700 it must use the on-chip SRAM. It now patches
  `MB2CR 0x05 -> 0x43`. The resulting program (`flashtest.c`) runs correctly
  when loaded with `--ram`.

## 9. Flash-boot investigation (open)

A 59-byte **beacon** (`flashbeacon.s`) was placed at flash offset 0. It runs at
the reset clock and emits `X` on serial A (no MMU/clock changes). In Run Mode
(JP1 removed, power-cycled) it produced **0 bytes at every baud 300-115200 and
both RTS states** — so the CPU is not executing flash offset 0.

Reading logical `0x0000` from a RAM program with `MB0CR=/CS0` also returned
`0xFF`, while reading the same flash through the data-segment window at
physical `0x100000` returned the expected bytes. So the flash is reachable via
the window but not via the root segment at reset.

`DCRabbit_10` research (see [status-and-todo.md](status-and-todo.md#what-the-dynamic-c-10-sources-say-reference))
shows reset `PC=0` should fetch from `/CS0` flash **only if `SYSCFG0` is low**.
If `SYSCFG0` is strapped high, reset MB0 is `/CS3` (internal SRAM, 16-bit) and
the CPU starts in SRAM instead — which would explain the beacon result. This
needs the RCM5700 module schematic to confirm. Other candidates: the
programming cable/JP1 holding SMODE, or an invalid ID block.

## 10. Dynamic C 10 research and the SMODE problem

### 10a. What DC10's sources establish

Read of `DCRabbit_10` (see [status-and-todo.md](status-and-todo.md) for the
full reference):

* For a parallel-flash RCM5700 the BIOS root code is at **physical flash
  offset 0** (`memory_layout.lib:311-330`, `ORG_FLASH_START 0x0`) and the first
  emitted code there is `_biosentry_` (`StdBios.c:1556`): `MACR=0x00` + 2 nops,
  `MMIDR=0x80`, `EDMR=0xC0`. The serial-flash triplet trampoline and the 16-bit
  trampoline are both compiled out for the RCM5700.
* **Nothing writes `MB0CR` before the first fetch.** The BIOS *assumes* reset
  already maps `/CS0` into bank 0. On the Rabbit 5000 that is selected by the
  `SYSCFG0`/`SYSCFG1` pins (`SYSCFG0=0` -> MB0 = `/CS0`, 8-bit, 4 wait;
  `SYSCFG0=1` -> MB0 = `/CS3` internal SRAM, 16-bit). `SYSCFG` appears nowhere
  in the DC10 tree - it is hardware only.
* `dkSetMMU` then sets `MECR=0x20`, `SEGSIZE=0xD6`, `DATASEGL/H=0x0100`,
  `MB0CR=0x00`, `MB1CR=0x00`, `MB2CR=0xC3`, `MB3CR=0x00`.
* The only place that explicitly re-maps bank 0 to flash and jumps to 0 is the
  pilot, after a host bootstrap (`ColdBoot/PILOT.C:650-668`).
* The ID-block marker is `55 AA 55 AA 55 AA` (`IDBLOCK.LIB:104`); the board
  read `4A 00 4A 00 4A 00` at `0xFFFFA`, so the ID block is not valid.
* DC10 cannot be ported to Linux: the compiler is closed-source Windows, and
  the library sources are Z-World-specific (see status-and-todo.md).

### 10b. SMODE is not under RTS control on this board

The serial beacon (`flashbeacon.s`) was replaced by a **baud-independent**
STATUS test: `statusbeacon.s` drives STATUS high and loops; `statusblink.s`
toggles STATUS slowly; `statusread.py` reads STATUS via the cable's DSR (the
same line Dynamic C uses). Results in "Run Mode" (RTS asserted):

```
statusbeacon: DSR high (STATUS not driven) -> offset-0 code did not run
statusblink : one transient, then steady -> no toggle
```

Crucially, after a Run-Mode reset the target still **executed bootstrap
triplets**: sending `GOCR=0x20` then `GOCR=0x30` via the triplet protocol made
DSR follow the writes exactly. In Run Mode the bootstrap ROM is not running, so
the triplets would be ignored. Conclusion: **the board was in bootstrap
(Program Mode) during the tests, and asserting RTS does not switch it to Run
Mode.** The mode is strapped by JP1, not (reliably) by the cable.

This invalidates the earlier "Run Mode" beacon tests: flash offset 0 was never
executed because the CPU was waiting in the bootstrap ROM. The earlier
"JP1 removed, power-cycled" test may have been defeated by the cable still
holding SMODE.

### 10c. Consequences

* The next boot test must guarantee true Run Mode (JP1 + no cable influence),
  ideally powered from the AC adapter with a separate serial connection.
* A definitive, cable-independent test is to have the offset-0 image erase and
  program a marker into a spare flash sector and read it back with the
  programmer.

## 11. Breakthrough: flash boot works in true Run Mode

The RCM5700/RCM6700 User's Manual (90001191) settled the mode question:

* **Program Mode** = SMODE pins pulled to +3.3 V, which happens when **JP1 pins
  1-2 are jumpered**.
* **Run Mode** = remove the JP1 pins 1-2 jumper; SMODE is pulled low and the
  Rabbit 5000 boots from flash after reset.
* With the **USB cable** supplying power in Run Mode, the MiniCore **reboots
  ~2 s after startup**. Use the **AC adapter / 5 V on J6**, or a power-only USB
  cable. J6 input is **+5 V DC** (module runs on 3.3 V from the on-board
  regulator).

So RTS never controlled SMODE (section 10b) - JP1 does. With **JP1 removed**
and the board powered from **J6**, `rcm5700/bootbeacon.s` (offset-0 image that
toggles STATUS) made the cable's DSR toggle continuously (~20 ms) - the CPU is
executing flash offset 0. A `statusbeacon` image (STATUS held high) reads as a
steady DSR. The earlier "0 bytes" results were all bootstrap-mode runs.

## 12. Open issue: no serial from a flash boot

`boothello.s` is a hand-written offset-0 image that replicates the DC BIOS
init (`MACR`/`MMIDR`/`EDMR`, `MECR`/`SEGSIZE`/`DATASEG`, `MB0..3CR`,
`GCSR=0x08`, `MTCR=0x0C`, `GCDR=0x07`) and then sets up serial A
(`PCFR=0x40`, `TACR=0`, `TAPR=1`, `TACSR=1`, `TAT4R=40`, `SACR=0x01`) and
prints `HI`. It **runs** (STATUS goes high) but emits **no bytes** on any baud
300-115200, even with the `SASR` wait removed. So either the Timer A4 baud
clock is still not running, or serial A TX is not reaching the cable in Run
Mode. This is the next thing to solve; STATUS is currently the only reliable
output channel from a flash boot.

`ramhello-flash.bin` (SDCC, built with `buildflash.sh`) also does not run from
a cold flash boot: the SDCC crt0 only sets `MB2CR`/`SEGSIZE`/`STACKSEG` and
relies on a BIOS to set `MECR`, the bank registers and the clock. A
DC-BIOS-style preamble in front of SDCC programs is the next step.

## 13. Dynamic C under Wine, and the flash-boot result (corrects section 11)

Dynamic C 10.72E was installed under Wine (silent NSIS) and the CLI compiler
works. A **targetless** build recipe produces a real RCM5700 image with the
Dynamic C BIOS; see [dynamic-c-wine.md](dynamic-c-wine.md). DC-built images
flash and verify correctly with OpenRabbit.

They do **not** run from flash, and neither does a bare asm image. The
definitive test used the **DS1 LED on PD0** (independent of serial and of the
STATUS/DSR line):

* `rcm5700/ledblink.s` loaded into RAM by the pilot -> **LED blinks** (so the
  LED, the pin, and the program are correct).
* The same image flashed at offset 0, board reset (JP1 removed, J6 power) ->
  **no blink**. DC's `FLASHLED01.bin` (with the real BIOS) also does not blink.
* `statusflash.c` (DC) and `bootbeacon.s` (asm) appeared to toggle STATUS, but
  `dchello.c` (which never touches STATUS/GOCR) toggles it identically in Run
  Mode. **The STATUS/DSR toggle is an artifact in Run Mode** and must not be
  used as a "program ran" indicator.

So **section 11's conclusion was wrong**: flash offset 0 is *not* executed on
reset, in either mode, with either a RESET-button or a full power-cycle. The
open question is why. Leading candidates: the reset memory mapping
(`SYSCFG0` strapping), the flash chip select or address inversion, or the reset
vector being at the top of the top-boot flash. See
[status-and-todo.md](status-and-todo.md) for the full analysis and the
cable-independent **flash-marker test** that will settle it.

## Dead ends worth remembering

* **The STATUS/DSR line is not a valid "program ran" signal in Run Mode** - it
  toggles even for a program that never writes GOCR. Only the LED (PD0) or a
  flash self-write marker are trustworthy.
* The FT232R + hub USB drop is real and needs a direct port / better adapter.
* `--ramcr` only touches MB0CR/MB1CR; 16-bit mode is in MACR. (Not relevant for
  the RCM5700, which is 8-bit.)
* Classic `DATASEG` silently truncates >8-bit pages; use DATASEGL/H.
* The `ramcr` value must be `0x43` for the RCM5700; the default `0x45` hangs at
  the pilot handshake.
* The FT232R/JP1 SMODE control is flaky without the physical jumper; do not
  rely on RTS alone.
* Hand-written asm must put a `nop` after every I/O access (SDCC does this for
  the Rabbit BSI/IOI erratum); without it the register writes are unreliable.
