; boothello.s - full DC-BIOS-style init at flash offset 0, then print on
; serial A. Replicates the essential parts of StdBios.c _biosentry_ + dkSetMMU
; so a cold flash boot has a valid MMU, clock and serial setup.
WDTTR    .equ 0x09
GCSR     .equ 0x00
GOCR     .equ 0x0e
GCDR     .equ 0x0f
MMIDR    .equ 0x10
SEGSIZE  .equ 0x13
MB0CR    .equ 0x14
MB1CR    .equ 0x15
MB2CR    .equ 0x16
MB3CR    .equ 0x17
MECR     .equ 0x18
MTCR     .equ 0x19
MACR     .equ 0x1d
DATASEGL .equ 0x1e
DATASEGH .equ 0x1f
PCFR     .equ 0x55
TAT4R    .equ 0xa9
TACSR    .equ 0xa0
TAPR     .equ 0xa1
TACR     .equ 0xa4
SADR     .equ 0xc0
SASR     .equ 0xc3
SACR     .equ 0xc4
EDMR     .equ 0x420

    .area _BOOT (ABS)
    .org 0x0000

    ; --- _biosentry_ ---
    xor a
    ioi
    ld  (MACR), a          ; 8-bit /CS0 & /CS1
    nop
    nop
    ld  a, #0x80
    ioi
    ld  (MMIDR), a         ; 15-bit I/O, R4K instruction set
    ld  a, #0xc0
    ioi
    ld  (EDMR), a

    ; --- watchdog off ---
    ld  a, #0x51
    ioi
    ld  (WDTTR), a
    ld  a, #0x54
    ioi
    ld  (WDTTR), a

    ; --- global clock ---
    ld  a, #0xa0
    ioi
    ld  (GOCR), a
    ld  a, #0x08
    ioi
    ld  (GCSR), a

    ; --- dkSetMMU ---
    ld  a, #0x20
    ioi
    ld  (MECR), a          ; 512 KB banks
    ld  a, #0xd6
    ioi
    ld  (SEGSIZE), a
    ld  hl, #0x0100
    ioi
    ld  (DATASEGL), hl

    xor a
    ioi
    ld  (MB0CR), a         ; /CS0 flash, 4 wait
    ld  (MB1CR), a
    ld  a, #0xc3
    ioi
    ld  (MB2CR), a         ; /CS3 internal SRAM, 0 wait
    xor a
    ioi
    ld  (MB3CR), a

    ; --- clock doubler (early OE + doubler) ---
    ld  a, #0x0c
    ioi
    ld  (MTCR), a
    ld  a, #0x07
    ioi
    ld  (GCDR), a

    ; --- serial A @ 38400 (50 MHz / 2 / 16 / 41) ---
    ld  a, #0x40
    ioi
    ld  (PCFR), a          ; PC6 = TXA
    ld  a, #0x01
    ioi
    ld  (TACSR), a         ; make timer A tick
    xor a
    ioi
    ld  (TACR), a          ; timer A4 clocked by main clock
    ld  a, #0x01
    ioi
    ld  (TAPR), a          ; timer A uses peripheral clock / 2
    ld  a, #40
    ioi
    ld  (TAT4R), a
    ld  a, #0x01
    ioi
    ld  (SACR), a          ; async, 8-bit

    ; STATUS high: proves we got here
    ld  a, #0x30
    ioi
    ld  (GOCR), a

loop:
    ld  a, #'H'
    ioi
    ld  (SADR), a
    nop
    ld  bc, #0x0000
1$:
    dec bc
    ld  a, b
    or  a, c
    jr  nz, 1$
    ld  a, #'I'
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
