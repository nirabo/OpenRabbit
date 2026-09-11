// RCM5700 (Rabbit 5000) parallel flash programmer - on-target self test.
// Runs from internal RAM (loaded by the pilot). Drives the S29AL008D on /CS0
// through bank 2, reached via the data segment window.

#include <stdint.h>
#include <stdio.h>

#include "r2k.h"
#include "targetconfigurations.h"

__sfr __at(0x18) MECR;
__sfr __at(0x1E) DATASEGL;
__sfr __at(0x1F) DATASEGH;

#define WIN ((volatile unsigned char *)0x8000)
#define FLASH_BASE_PHYS 0x100000UL	// bank 2 -> flash offset 0

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

// Map the 8 KB data segment window onto the 8 KB flash page containing `off`.
// Lower 512 KB live in bank 2 (physical 0x100000+off), upper 512 KB in bank 3
// (physical 0x180000 + off-0x80000); the flash only sees A19..A0.
static void set_window(unsigned long off)
{
	unsigned long base = off & ~0x1FFFUL;
	unsigned long phys = (base < 0x80000UL) ? (0x100000UL + base) : (0x180000UL + (base - 0x80000UL));
	unsigned long page = phys >> 12;
	DATASEGL = (unsigned char)(page & 0xFF);
	DATASEGH = (unsigned char)((page >> 8) & 0x0F);
}

static unsigned char fread(unsigned long off)
{
	set_window(off);
	return WIN[off & 0x1FFF];
}

static void fwrite(unsigned long off, unsigned char v)
{
	set_window(off);
	WIN[off & 0x1FFF] = v;
}

// Wait until two consecutive reads of `off` are equal (flash settled).
static unsigned char fpoll(unsigned long off)
{
	unsigned char a, b;
	unsigned int guard = 0;
	do {
		a = fread(off);
		b = fread(off);
	} while(a != b && ++guard);
	return b;
}

static void erase_sector(unsigned long off)
{
	fwrite(0xAAA, 0xAA);
	fwrite(0x555, 0x55);
	fwrite(0xAAA, 0x80);
	fwrite(0xAAA, 0xAA);
	fwrite(0x555, 0x55);
	fwrite(off, 0x30);
	fpoll(off);
}

static void program_byte(unsigned long off, unsigned char d)
{
	fwrite(0xAAA, 0xAA);
	fwrite(0x555, 0x55);
	fwrite(0xAAA, 0xA0);
	fwrite(off, d);
	fpoll(off);
}

static void flash_reset(void)
{
	fwrite(0xAAA, 0xF0);
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
		while(rtclock() - c < 32 * 100);
	}

	printf("\nRCM5700 flash self-test\n");

	MECR = 0x20;		// 512 KB banks
	MB2CR = 0x00;		// bank 2 = /CS0 flash, 4 wait, writable
	MB3CR = 0x00;		// bank 3 = /CS0 flash (upper half)

	// Autoselect ID
	fwrite(0xAAA, 0xAA);
	fwrite(0x555, 0x55);
	fwrite(0xAAA, 0x90);
	unsigned char mfg = fread(0x00);
	unsigned char dev = fread(0x02);
	flash_reset();
	printf("ID: mfg="); hex8(mfg); printf(" dev="); hex8(dev); printf(" (expect 01 DA)\n");

	// Read original contents of sector 0 (first bytes) and the ID block marker at top.
	printf("before erase @0: ");
	for(unsigned int i = 0; i < 8; i++) { hex8(fread(i)); putchar(' '); }
	printf("\n");
	printf("ID block marker @0xFFFFA: ");
	for(unsigned long i = 0xFFFFA; i <= 0xFFFFF; i++) { hex8(fread(i)); putchar(' '); }
	printf("(expect 55 AA 55 AA 55 AA)\n");

	// Erase sector 0 and verify blank.
	printf("erasing sector 0...\n");
	erase_sector(0);
	printf("after erase @0: ");
	for(unsigned int i = 0; i < 8; i++) { hex8(fread(i)); putchar(' '); }
	printf("\n");

	// Program a pattern and read it back.
	printf("programming...\n");
	program_byte(0, 0x5A);
	program_byte(1, 0xA5);
	program_byte(2, 0x00);
	program_byte(3, 0xFF);
	printf("after program @0: ");
	for(unsigned int i = 0; i < 4; i++) { hex8(fread(i)); putchar(' '); }
	printf("(expect 5A A5 00 FF)\n");

	flash_reset();
	printf("done\n");

	for(;;);
}
