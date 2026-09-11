# RCM5700W hardware notes

## Module

* Digi/Rabbit **RCM5700W** (the `W` denotes the Wi-Fi variant of the RCM5700).
* CPU: **Rabbit 5000**, 25.000 MHz crystal with clock doubler enabled (0x07) → 50 MHz.
* **128 KB on-chip SRAM** — the RCM5700 has **no external RAM**.
* **1 MB parallel flash** (U3).
* USB programming via the official FT232R cable (`0403:6001`, `/dev/ttyUSB0`).

CPU ID reported by OpenRabbit: `0x0300 (Rabbit 5000)`, `Freq: 25000000`.

## Flash

The installed part is a **Spansion S29AL008D** (JEDEC manufacturer/device
`01 DA`). It is 8 Mbit = 1 MB, organised 1 M × 8 / 512 K × 16, and is a
**top-boot** device.

Top-boot sector layout (from bottom to top, 256 × 4 KB blocks):

| Sectors | Size |
|---|---|
| 15 × | 64 KB |
| 1 ×  | 32 KB |
| 2 ×  | 8 KB |
| 1 ×  | 16 KB |

The flash is connected to **/CS0, /OE0, /WE0** and is used in **8-bit** mode.
JEDEC byte-mode command addresses are **0xAAA / 0x555**.

## Memory map

The RCM5700 firmware uses `MECR = 0x20` (bank select on A[20:19], 512 KB banks):

| Bank | Physical range | Device |
|---|---|---|
| MB0 | 0x000000–0x07FFFF | /CS0 flash (low half) |
| MB1 | 0x080000–0x0FFFFF | /CS0 flash (high half) |
| MB2 | 0x100000–0x17FFFF | (free) |
| MB3 | 0x180000–0x1FFFFF | /CS3 on-chip SRAM |

The flash only decodes A[19:0], so it aliases across banks:

* physical `0x100000` (MB2, A19:0 = 0) → flash offset `0x00000`
* physical `0x080000` (MB1, A19 = 1) → flash offset `0x80000`
* physical `0x000000` (MB0, at reset) → flash offset `0x00000`

This is why the programmer can reach the whole 1 MB flash through banks 2 and 3
while its own code/stack live in the on-chip SRAM.

The on-chip SRAM is selected by **/CS3** and was verified to respond in MB1, MB2
and MB3 (see `rcm5700/memtest.c`). It is 128 KB, so higher physical addresses
alias it.

## Register gotchas

* **`MECR` (0x18)** is not defined by SDCC's `r2k.h`; declare it with
  `__sfr __at(0x18) MECR;`.
* The classic **`DATASEG` (0x12)** register is only 8 bits (physical bits
  19:12). To point a segment at a physical address ≥ 1 MB you must use the
  Rabbit 4000+ pair **`DATASEGL` (0x1E) / `DATASEGH` (0x1F)**, which together
  form a 12-bit page number. Writing `0x100` to `DATASEG` silently truncates to
  `0`.
* **`MMIDR` (0x10)** bit 7 must be set to decode internal I/O addresses ≥ 0x0100
  (e.g. `ACS0CR` at 0x0410). The 8-bit flash path does not need those.

## Observed flash contents (test board)

```
0x00000–0x6FFFF   firmware (BIOS + program)
0x70000–0xEFFFF   blank (0xFF)
0xF8000–0xFFFFF   user block + ID block (top of flash)
```

The ID block ends at physical `0x0FFFFF`. OpenRabbit's `SysIDBlockType`
(`src/mytypes.h`) is the older v1–v4 layout (no `idBlock2`), so the trailing
fields of a v5 ID block are misparsed; the leading fields (flash ID/type/size/
sector/speed, CPU ID, crystal frequency) parse correctly.

Values read from the test board:

```
CPU:  0x0300 (Rabbit 5000)
Freq: 25000000
sectorSize: 0x0080
numSectors: 0x0013
flashSize:  0x0080
Flash speed: 70 ns
RAM speed: 15 ns
```
