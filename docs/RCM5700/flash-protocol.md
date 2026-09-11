# RAM programmer serial protocol and flash algorithm

The programmer is `rcm5700/rcmprog.c` (prebuilt: `rcm5700/rcmprog.bin`). It runs
from the Rabbit 5000's on-chip SRAM, loaded and started by OpenRabbit via the
`--programmer` option.

## Serial link

38400 baud, 8 data bits, no parity, 1 stop bit, no flow control, raw bytes.

All multi-byte integers are **little-endian**.

| Host sends | Programmer replies |
|---|---|
| `0x55` | `0xAA` (sync) |
| `'E'` `<u32 size>` | `0x06` after erasing sectors covering `[0, size)` |
| `'W'` `<u32 addr>` `<u16 len>` `<len bytes>` | `0x06` after programming |
| `'R'` `<u32 addr>` `<u16 len>` | `<len bytes>` read from flash |
| anything else | `0x15` (NAK) |

The host (`rcm5700_flash()` in `src/rabbit.c`) syncs by sending `0x55` until it
receives `0xAA`, erases once, then programs and verifies in 128-byte chunks.

## Flash access from the programmer

The programmer runs from the on-chip SRAM (selected by /CS3). It maps the flash
into banks so that it stays reachable without relocating itself:

```
MECR   = 0x20        ; 512 KB banks
MB2CR  = 0x00        ; bank 2 = /CS0 flash, 4 wait, /OE0/WE0, writable
MB3CR  = 0x00        ; bank 3 = /CS0 flash (upper half)
```

An 8 KB **data-segment window** (logical 0x8000–0x9FFF) is pointed at the
required flash page using `DATASEGL`/`DATASEGH`:

```
flash offset 0x00000..0x7FFFF  -> physical 0x100000 + off   (bank 2)
flash offset 0x80000..0xFFFFF  -> physical 0x180000 + (off-0x80000)  (bank 3)
page = physical >> 12 ; DATASEGL = page & 0xFF ; DATASEGH = page >> 8
```

Because the S29AL008D treats A18–A11 as "don't care" during unlock/command
cycles, only the low address bits of the JEDEC command addresses matter, and the
bank/base offset does not break the command sequences.

## JEDEC byte-mode sequences (S29AL008D)

Addresses are flash offsets.

* **Read ID:** `AA`@0xAAA, `55`@0x555, `90`@0xAAA, then read
  manufacturer at 0x00 (`01`) and device at 0x02 (`DA`), then `F0`@0xAAA.
* **Sector erase:** `AA`@0xAAA, `55`@0x555, `80`@0xAAA, `AA`@0xAAA, `55`@0x555,
  `30`@SA.
* **Program byte:** `AA`@0xAAA, `55`@0x555, `A0`@0xAAA, `PD`@PA.
* **Reset:** `F0` at any address.

Completion is detected by polling the programmed/erased address until two
consecutive reads are equal (stable-data polling), with a guard counter.

## Sector map (top-boot)

`erase_range(0, size)` erases every sector that intersects `[0, size)`:

```
sizes in 4 KB blocks, bottom to top:
16,16,16,16,16,16,16,16,16,16,16,16,16,16,16, 8, 2, 2, 4
= 15×64KB, 1×32KB, 2×8KB, 1×16KB  (= 1 MB)
```

## Reference

The algorithm is a from-scratch reimplementation of the relevant parts of Digi's
`FLASHWR.LIB` (Dynamic C 10), which is the authoritative reference:
`github.com/digidotcom/DCRabbit_10`, `Lib/Rabbit4000/BIOSLIB/FLASHWR.LIB`.
