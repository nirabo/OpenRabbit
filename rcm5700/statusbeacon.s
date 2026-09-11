; statusbeacon.s - minimal test to be placed at flash offset 0.
;
; If the CPU actually executes flash offset 0 in Run Mode, this drives the
; STATUS pin high (GOCR=0x30) and loops forever. STATUS is wired to the
; programming cable's DSR line, so the test is baud-independent - this is the
; same technique Dynamic C uses to detect a target (see
; DCRabbit_10/ColdBoot/CHECKCS04mem.C and OpenRabbit src/rabbit.c).
;
; Deliberately does NOT touch the MMU or clock: if the reset mapping puts the
; flash at /CS0, a few instructions at offset 0 are all that is needed.
;
; Read the result with rcm5700/statusread.py.
WDTTR   .equ 0x09
GOCR    .equ 0x0e

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

    ; STATUS high: proves the CPU reached this code
    ld  a, #0x30
    ioi
    ld  (GOCR), a
    nop
1$:
    jr  1$
