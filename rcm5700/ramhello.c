// Minimal RAM-run test for the RCM5700 (Rabbit 5000).
// Signalling STATUS and printing over serial port A @ 38400 baud.
// Unlike the flash examples, this does NOT reconfigure MB0CR/MB1CR, because the
// program runs from the on-chip SRAM that is mapped there.

#include <stdint.h>
#include <stdio.h>

#include "r2k.h"

#include "targetconfigurations.h"

#if __SDCC_REVISION >= 13762
unsigned char __sdcc_external_startup(void)
#else
unsigned char _sdcc_external_startup(void)
#endif
{
	// Disable watchdog
	WDTTR = 0x51;
	WDTTR = 0x54;

	// normal oscillator, processor and peripheral from main clock, no periodic interrupt
	GCSR = 0x08;

	GCDR = CLOCK_DOUBLER;

	return 0;
}

int putchar(int c)
{
	if (c == '\n') {
		putchar('\r');
	}

	while(SASR & 0x04);
	SADR = c;
	return c;
}

void main(void)
{
	GOCR = 0x30;	// STATUS high signals to OpenRabbit that the user program is running.

	PCFR = 0x40;	// Use pin PC6 as TXA

	TAT4R = SERIAL_DIVIDER_38400 - 1;
	TACSR = 0x01;	// Enable timer A

	SACR = 0x00;	// No interrupts, 8-bit async mode

	for(;;) {
		printf("RCM5700 RAM OK\n");
	}
}
