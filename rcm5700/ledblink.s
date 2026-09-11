; ledblink.s - bare offset-0 image (no BIOS) that blinks the Interface Board
; LED on PD0. Independent of serial and of the STATUS/DSR line.
WDTTR   .equ 0x09
PDDR    .equ 0x60
PDFR    .equ 0x65
PDDDR   .equ 0x67

    .area _BOOT (ABS)
    .org 0x0000

    ; watchdog off
    ld  a, #0x51
    ioi
    ld  (WDTTR), a
    ld  a, #0x54
    ioi
    ld  (WDTTR), a

    ; PD0 as plain output
    xor a
    ioi
    ld  (PDFR), a
    ld  a, #0x01
    ioi
    ld  (PDDDR), a

loop:
    ld  a, #0x01
    ioi
    ld  (PDDR), a
    ld  bc, #0x0000
1$:
    dec bc
    ld  a, b
    or  a, c
    jr  nz, 1$
    xor a
    ioi
    ld  (PDDR), a
    ld  bc, #0x0000
2$:
    dec bc
    ld  a, b
    or  a, c
    jr  nz, 2$
    jr  loop
