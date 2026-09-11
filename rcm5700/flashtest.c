// RCM5700 flash-run test: set up clock + serial A and print a banner.
// Built with buildflash.sh (stack/data in on-chip SRAM).

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

	// Main clock (with doubler), processor and peripheral from main clock
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
	GOCR = 0x30;	// STATUS high: tells OpenRabbit the program is running
	PCFR = 0x40;	// PC6 as TXA
	TAT4R = SERIAL_DIVIDER_38400 - 1;
	TACSR = 0x01;	// Enable timer A
	SACR = 0x00;	// 8-bit async mode

	for(;;) {
		printf("RCM5700 FLASH OK\n");
	}
}
