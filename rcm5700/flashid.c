// Stage 2a: from a RAM program on the RCM5700, map the S29AL008D flash into
// bank 2 (/CS0) and read its JEDEC manufacturer/device ID.
//
// The flash is 1 MB on /CS0. With MECR=0x20 (512 KB banks) bank 2 covers
// physical 0x100000-0x17FFFF, and because the flash only has A19..A0, that
// aliases to flash offsets 0x00000-0x7FFFF. We reach it through the unused
// data segment (logical 0x8000-0x9FFF) by setting DATASEG=0x100.

#include <stdint.h>
#include <stdio.h>

#include "r2k.h"
#include "targetconfigurations.h"

__sfr __at(0x18) MECR;
__sfr __at(0x1D) MACR;
__sfr __at(0x19) MTCR;
__sfr __at(0x1E) DATASEGL;
__sfr __at(0x1F) DATASEGH;

#if __SDCC_REVISION >= 13762
unsigned char __sdcc_external_startup(void)
#else
unsigned char _sdcc_external_startup(void)
#endif
{
	WDTTR = 0x51;
	WDTTR = 0x54;
	GCSR = 0x08;
	GCDR = CLOCK_DOUBLER;
	return 0;
}

int putchar(int c)
{
	if (c == '\n') putchar('\r');
	while(SASR & 0x04);
	SADR = c;
	return c;
}

static void hex8(unsigned char v)
{
	const char *h = "0123456789ABCDEF";
	putchar(h[v >> 4]);
	putchar(h[v & 15]);
}

static unsigned long rtclock(void)
{
	unsigned long a, b;
	do {
		RTC0R = 0;
		a = ((unsigned long)RTC0R) | ((unsigned long)RTC1R << 8) | ((unsigned long)RTC2R << 16) | ((unsigned long)RTC3R << 24);
		b = ((unsigned long)RTC0R) | ((unsigned long)RTC1R << 8) | ((unsigned long)RTC2R << 16) | ((unsigned long)RTC3R << 24);
	} while(a != b);
	return b;
}

void main(void)
{
	GOCR = 0x30;
	PCFR = 0x40;
	TAT4R = SERIAL_DIVIDER_38400 - 1;
	TACSR = 0x01;
	SACR = 0x00;

	{
		unsigned long c = rtclock();
		while(rtclock() - c < 32 * 100);	// 100 ms for the host to switch baud
	}

	printf("MECR=%02x MB0=%02x MB1=%02x MB2=%02x MB3=%02x MMIDR=%02x\n", MECR, MB0CR, MB1CR, MB2CR, MB3CR, MMIDR);
	printf("SEGSIZE=%02x DATASEG=%02x STACKSEG=%02x\n", SEGSIZE, DATASEG, STACKSEG);

	// Use 512 KB banks: MB0=0x000000 (RAM/code), MB1=0x080000, MB2=0x100000, MB3=0x180000.
	// Then bank 2 (physical 0x100000) aliases the flash's low 512 KB (A19..A0=0).
	MECR = 0x20;

	// Map bank 2 to /CS0 flash: 4 wait states, /OE0 /WE0, /CS0, writable.
	MB2CR = 0x00;

	// Point the data segment at physical 0x100000 (flash offset 0).
	// The Rabbit 4000+ segment base is 12 bits: DATASEGH:DATASEGL = 0x100.
	DATASEGL = 0x00;
	DATASEGH = 0x01;

	volatile unsigned char *f = (volatile unsigned char *)0x8000;

	// JEDEC byte-mode autoselect: AA@AAA, 55@555, 90@AAA, read ID.
	f[0xAAA] = 0xAA;
	f[0x555] = 0x55;
	f[0xAAA] = 0x90;
	unsigned char mfg = f[0x00];
	unsigned char dev = f[0x02];
	f[0xAAA] = 0xF0;	// reset to read mode

	printf("flash ID: mfg=%02x dev=%02x (expect 01 DA)\n", mfg, dev);

	for(;;);
}
