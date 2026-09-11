; Minimal RCM5700 flash test: emit 'A' forever on serial port A @ 38400.
WDTTR   .equ 0x09
GCSR    .equ 0x00
GCDR    .equ 0x0f
PCFR    .equ 0x55
TAT4R   .equ 0xa9
TACSR   .equ 0xa0
SACR    .equ 0xc4
SASR    .equ 0xc3
SADR    .equ 0xc0

    .area _CODE

    ld  a, #0x51
    ioi
    ld  (WDTTR), a
    ld  a, #0x54
    ioi
    ld  (WDTTR), a

    ld  a, #0x08
    ioi
    ld  (GCSR), a
    ld  a, #0x07
    ioi
    ld  (GCDR), a

    ld  a, #0x40
    ioi
    ld  (PCFR), a

    ld  a, #40
    ioi
    ld  (TAT4R), a
    ld  a, #0x01
    ioi
    ld  (TACSR), a
    xor a
    ioi
    ld  (SACR), a

loop:
    ld  a, #0x41
    ioi
    ld  (SADR), a
txwait:
    ioi
    ld  a, (SASR)
    and a, #0x04
    jr  nz, txwait
    jr  loop
