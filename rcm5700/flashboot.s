; flashboot.s - boot a program from parallel flash on the RCM5700.  [WIP]
;
; Loaded into the on-chip SRAM with `--ram` and started by the pilot at 0x0000.
; The stub maps the stack segment to on-chip SRAM (MB2CR=/CS3) and jumps to a
; copy of the remap code at logical 0xb000 (physical 0x81000 -> MB2 -> SRAM
; offset 0x1000). That code runs with MB0 still on RAM, switches MB0 to /CS0
; flash and jumps to flash offset 0. Kept under 16 KB so it does not clobber
; the pilot at physical 0x4000.
;
; STATUS: the jump/remap executes, but reading flash at logical 0x0000 with
; MB0CR=/CS0 returns 0xFF, so this does not yet boot the flashed image. See
; docs/RCM5700/status-and-todo.md.
;
; Note: every I/O access is followed by a NOP. SDCC does this on the Rabbit
; (BSI/IOI erratum) and without it the register writes below are unreliable.

WDTTR    .equ 0x09
GCSR     .equ 0x00
GCDR     .equ 0x0f
SEGSIZE  .equ 0x13
STACKSEG .equ 0x11
MB0CR    .equ 0x14
MB2CR    .equ 0x16
MECR     .equ 0x18
PCFR     .equ 0x55
TAT4R    .equ 0xa9
TACSR    .equ 0xa0
SACR     .equ 0xc4

    .area _BOOT (ABS)

    .org 0x0000
    ; disable watchdog
    ld  a, #0x51
    ioi
    ld  (WDTTR), a
    nop
    ld  a, #0x54
    ioi
    ld  (WDTTR), a
    nop

    ; clock + serial A at 38400 (for optional debug output)
    ld  a, #0x08
    ioi
    ld  (GCSR), a
    nop
    ld  a, #0x07
    ioi
    ld  (GCDR), a
    nop
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

    ; map the stack segment to on-chip SRAM via MB2 (0x80000-0xBFFFF)
    xor a
    ioi
    ld  (MECR), a
    nop
    ld  a, #0xa8
    ioi
    ld  (SEGSIZE), a
    nop
    ld  a, #0x76
    ioi
    ld  (STACKSEG), a
    nop
    ld  a, #0x43
    ioi
    ld  (MB2CR), a
    nop

    ; run the remap code from logical 0xb000 (physical 0x81000 -> SRAM 0x1000)
    jp  0xb000

    .org 0x1000
remap:
    ; disable the clock doubler, then map MB0 to /CS0 flash (DCRabbit_10
    ; PILOT.C _PB_StartRegBios does the same)
    xor a
    ioi
    ld  (GCDR), a
    nop
    xor a
    ioi
    ld  (MB0CR), a
    nop

    ; Touch flash at 0x0000: drops any prefetched bytes from the old mapping.
    ld  hl, #0x0000
1$:
    ld  a, (hl)
    cp  (hl)
    jr  nz, 1$
    cp  (hl)
    jr  nz, 1$

    ; run the flashed program
    jp  (hl)
