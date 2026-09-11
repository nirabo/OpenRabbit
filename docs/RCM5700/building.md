# Building the RCM5700 helper programs

## Requirements

* SDCC (tested with 4.5.0) providing `sdcc` and `sdasrab`
* binutils `objcopy`
* the OpenRabbit examples directory (for `targetconfigurations.h`)

## Generic command line

```
sdcc -mr2k -I../examples --data-loc 0xa000 -DRCM5700 -c prog.c -o prog.rel
sdcc -mr2k -I../examples --data-loc 0xa000 prog.rel -o prog.ihx
objcopy -I ihex -O binary prog.ihx prog.bin
```

`-mr2k` targets the Rabbit 2000/3000/4000; the Rabbit 5000 runs this code in
compatibility mode. `-DRCM5700` selects the RCM5700 values in
`examples/targetconfigurations.h` (`SERIAL_DIVIDER_38400 = 41`,
`CLOCK_DOUBLER = 0x07`).

## The SDCC crt0 problem (important)

SDCC's Rabbit startup code (`/usr/share/sdcc/lib/src/r2k/crt0.s`) assumes a
board with external RAM:

```
ld a,#0x05 ; ioi ; ld (MB2CR),a      ; MB2CR = /CS1
ld a,#0x76 ; ioi ; ld (STACKSEG),a   ; stack at physical 0x76000
```

The RCM5700 has **no external RAM**, so physical 0x76000 is not valid and the
program dies on the first `call` (the stack is used before
`__sdcc_external_startup` runs). The two build scripts patch the linked binary
to fix this. The relevant header bytes are at fixed offsets:

| offset | original | meaning |
|---|---|---|
| 0x04 | `3e 05 d3 32 16 00` | `ld a,#0x05; ioi; ld (MB2CR),a` |
| 0x10 | `3e 76 d3 32 11 00` | `ld a,#0x76; ioi; ld (STACKSEG),a` |

### RAM programs — `build.sh`

Patches **STACKSEG 0x76 → 0x18** (physical 0x18000, on-chip SRAM via the
/CS3 mapping set up by coldload). Used for programs loaded with `--ram`.

```
./build.sh rcmprog      # -> rcmprog.bin
./build.sh ramhello     # -> ramhello.bin
```

### Flash programs — `buildflash.sh`

Patches **MB2CR → MB1CR = 0x43** (`/CS3`) and **STACKSEG 0x76 → 0x40**
(physical 0x040000, on-chip SRAM via MB1). Used for programs intended to run
from flash.

```
./buildflash.sh memtest   # -> memtest-flash.bin
```

> Note: running flashed programs is still unresolved (see
> [status-and-todo.md](status-and-todo.md)); `buildflash.sh` produces a program
> that *should* have a valid stack on the RCM5700, but the boot path is not yet
> verified.

A cleaner long-term fix is a custom `crt0`/linker configuration for the RCM5700
rather than binary patching.

## Self test

`rcmflash.c` is a destructive on-target test that reads the flash ID, reads the
first bytes, erases sector 0, programs `5A A5 00 FF` and reads it back. Build
with `build.sh rcmflash` and run with `--ram`.

## Hand-written assembly

`tiny.s` is a 63-byte program that only sets up serial port A and emits `A`
forever. It is assembled/linked manually (it needs no crt0):

```
sdasrab -o tiny.rel tiny.s
sdcc -mr2k --code-loc 0 --no-std-crt0 tiny.rel -o tiny.ihx
objcopy -I ihex -O binary tiny.ihx tiny.bin
```
