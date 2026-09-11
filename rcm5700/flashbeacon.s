; flashbeacon.s - minimal test placed at flash offset 0. Runs at the reset clock
; (main/8 = 3.125 MHz) and emits 'X' at ~2400 baud, so the host can see whether
; the CPU executes flash offset 0 in Run Mode. Deliberately does NOT change the
; clock or MMU.
WDTTR   .equ 0x09
PCFR    .equ 0x55
TAT4R   .equ 0xa9
TACSR   .equ 0xa0
SACR    .equ 0xc4
SASR    .equ 0xc3
SADR    .equ 0xc0

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

    ; serial A @ ~2400 (reset clock)
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
    ld  a, #0x58          ; 'X'
    ioi
    ld  (SADR), a
    nop
1$:
    ioi
    ld  a, (SASR)
    nop
    and a, #0x04
    jr  nz, 1$
    jr  loop
