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

## Dead ends worth remembering

* The FT232R + hub USB drop is real and needs a direct port / better adapter.
* `--ramcr` only touches MB0CR/MB1CR; 16-bit mode is in MACR. (Not relevant for
  the RCM5700, which is 8-bit.)
* Classic `DATASEG` silently truncates >8-bit pages; use DATASEGL/H.
* The `ramcr` value must be `0x43` for the RCM5700; the default `0x45` hangs at
  the pilot handshake.
