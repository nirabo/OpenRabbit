// RCM5700 (Rabbit 5000) parallel flash programmer.
// Runs from internal RAM (loaded and started by OpenRabbit). Drives the
// Spansion S29AL008D on /CS0 through bank 2/3 via the data segment window.
//
// Host protocol (38400 8N1, raw):
//   0x55                 -> replies 0xAA (sync)
//   'E' <u32 size LE>    -> erase sectors covering [0,size), replies 0x06
//   'W' <u32 addr> <u16 len> <len bytes> -> program, replies 0x06
//   'R' <u32 addr> <u16 len>             -> replies <len bytes>
//   anything else        -> replies 0x15 (NAK)

#include <stdint.h>
#include <stdio.h>

#include "r2k.h"
#include "targetconfigurations.h"

__sfr __at(0x18) MECR;
__sfr __at(0x1E) DATASEGL;
__sfr __at(0x1F) DATASEGH;

#define WIN ((volatile unsigned char *)0x8000)

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

static unsigned char rdb(void)
{
	while(!(SASR & 0x80));
	return SADR;
}

static void wrb(unsigned char c)
{
	while(SASR & 0x04);
	SADR = c;
}

static unsigned long rd32(void)
{
	unsigned long v = rdb();
	v |= (unsigned long)rdb() << 8;
	v |= (unsigned long)rdb() << 16;
	v |= (unsigned long)rdb() << 24;
	return v;
}

static unsigned int rd16(void)
{
	unsigned int v = rdb();
	v |= (unsigned int)rdb() << 8;
	return v;
}

// S29AL008D top-boot sector sizes, in 4 KB units.
static const unsigned int sector_4k[] = {
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 8, 2, 2, 4
};

static void erase_range(unsigned long start, unsigned long end)
{
	unsigned long addr = 0;
	for(unsigned int s = 0; s < sizeof(sector_4k) / sizeof(sector_4k[0]); s++) {
		unsigned long sz = (unsigned long)sector_4k[s] * 4096UL;
		if(addr >= end)
			break;
		if(addr < end && addr + sz > start)
			erase_sector(addr);
		addr += sz;
	}
}

void main(void)
{
	GOCR = 0x30;
	PCFR = 0x40;
	TAT4R = SERIAL_DIVIDER_38400 - 1;
	TACSR = 0x01;
	SACR = 0x00;

	MECR = 0x20;		// 512 KB banks
	MB2CR = 0x00;		// bank 2 = /CS0 flash, 4 wait, writable
	MB3CR = 0x00;		// bank 3 = /CS0 flash (upper half)

	for(;;) {
		unsigned char cmd = rdb();
		switch(cmd) {
		case 0x55:
			wrb(0xAA);
			break;
		case 'E': {
			unsigned long size = rd32();
			erase_range(0, size);
			wrb(0x06);
			break;
		}
		case 'W': {
			unsigned long addr = rd32();
			unsigned int len = rd16();
			for(unsigned int i = 0; i < len; i++)
				program_byte(addr + i, rdb());
			wrb(0x06);
			break;
		}
		case 'R': {
			unsigned long addr = rd32();
			unsigned int len = rd16();
			for(unsigned int i = 0; i < len; i++)
				wrb(fread(addr + i));
			break;
		}
		case 'G':
			flash_reset();
			wrb(0x06);
			break;
		default:
			wrb(0x15);
			break;
		}
	}
}
