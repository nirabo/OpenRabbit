; statusblink.s - definitive test for "does the CPU execute flash offset 0?".
;
; Placed at flash offset 0, it alternates the STATUS pin high/low with a slow
; delay. If Run Mode reads STATUS toggling, the CPU is definitely executing
; flash offset 0 (a floating line cannot toggle). Read with
; rcm5700/statusread.py --repeat N.
WDTTR   .equ 0x09
GOCR    .equ 0x0e

    .area _BOOT (ABS)
    .org 0x0000

    ld  a, #0x51
    ioi
    ld  (WDTTR), a
    nop
    ld  a, #0x54
    ioi
    ld  (WDTTR), a
    nop

start:
    ld  a, #0x30          ; STATUS high
    ioi
    ld  (GOCR), a
    nop
    ld  bc, #0x0000
1$:
    dec bc
    ld  a, b
    or  a, c
    jr  nz, 1$

    ld  a, #0x20          ; STATUS low
    ioi
    ld  (GOCR), a
    nop
    ld  bc, #0x0000
2$:
    dec bc
    ld  a, b
    or  a, c
    jr  nz, 2$
    jr  start
