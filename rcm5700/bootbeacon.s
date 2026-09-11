; bootbeacon.s - placed at flash offset 0. If the CPU executes offset 0 it:
;   * toggles the STATUS pin slowly (baud-independent; read via cable DSR), and
;   * emits 'X' on serial A at the reset clock.
; Both signals together distinguish "code ran" from a floating line.
WDTTR   .equ 0x09
GOCR    .equ 0x0e
PCFR    .equ 0x55
TAT4R   .equ 0xa9
TACSR   .equ 0xa0
SADR    .equ 0xc0
SASR    .equ 0xc3
SACR    .equ 0xc4

    .area _BOOT (ABS)
    .org 0x0000

    ; watchdog off
    ld  a, #0x51
    ioi
    ld  (WDTTR), a
    nop
    ld  a, #0x54
    ioi
    ld  (WDTTR), a
    nop

    ; serial A at the reset clock
    ld  a, #0x40
    ioi
    ld  (PCFR), a
    nop
    ld  a, #40
    ioi
    ld  (TAT4R), a
    nop
    ld  a, #0x01
    ioi
    ld  (TACSR), a
    nop
    xor a
    ioi
    ld  (SACR), a
    nop

loop:
    ld  a, #0x30          ; STATUS high
    ioi
    ld  (GOCR), a
    nop
    ld  a, #0x58          ; 'X'
    ioi
    ld  (SADR), a
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
    ld  a, #0x58          ; 'X'
    ioi
    ld  (SADR), a
    nop
    ld  bc, #0x0000
2$:
    dec bc
    ld  a, b
    or  a, c
    jr  nz, 2$
    jr  loop
