// Scan the RCM5700 S29AL008D flash and report non-blank regions, to find the
// layout / boot area.
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
	WDTTR = 0x51; WDTTR = 0x54; GCSR = 0x08; GCDR = CLOCK_DOUBLER; return 0;
}

int putchar(int c){ if(c=='\n') putchar('\r'); while(SASR&0x04); SADR=c; return c; }
static void hex8(unsigned char v){ const char*h="0123456789ABCDEF"; putchar(h[v>>4]); putchar(h[v&15]); }
static void hex32(unsigned long v){ hex8(v>>24); hex8(v>>16); hex8(v>>8); hex8(v); }
static unsigned long rtclock(void){ unsigned long a,b; do{ RTC0R=0; a=RTC0R|((unsigned long)RTC1R<<8)|((unsigned long)RTC2R<<16)|((unsigned long)RTC3R<<24); b=RTC0R|((unsigned long)RTC1R<<8)|((unsigned long)RTC2R<<16)|((unsigned long)RTC3R<<24);}while(a!=b); return b; }

static void set_window(unsigned long off)
{
	unsigned long base = off & ~0x1FFFUL;
	unsigned long phys = (base < 0x80000UL) ? (0x100000UL + base) : (0x180000UL + (base - 0x80000UL));
	unsigned long page = phys >> 12;
	DATASEGL = (unsigned char)(page & 0xFF);
	DATASEGH = (unsigned char)((page >> 8) & 0x0F);
}
static unsigned char fread(unsigned long off){ set_window(off); return WIN[off & 0x1FFF]; }

void main(void)
{
	GOCR = 0x30; PCFR = 0x40; TAT4R = SERIAL_DIVIDER_38400 - 1; TACSR = 0x01; SACR = 0x00;
	{ unsigned long c=rtclock(); while(rtclock()-c < 32*100); }

	MECR = 0x20; MB2CR = 0x00; MB3CR = 0x00;

	printf("\nRCM5700 flash scan\n");
	for(unsigned long base = 0; base < 0x100000UL; base += 0x10000UL) {
		unsigned long first = 0xFFFFFFFFUL, nonblank = 0;
		for(unsigned long i = base; i < base + 0x10000UL; i += 0x1000) {
			set_window(i);
			for(unsigned int j = 0; j < 0x1000; j++) {
				unsigned char v = WIN[j];
				if(v != 0xFF) { if(first == 0xFFFFFFFFUL) first = i + j; nonblank++; }
			}
		}
		printf("@"); hex32(base); printf(" nonFF="); hex32(nonblank);
		if(first != 0xFFFFFFFFUL) { printf(" first="); hex32(first); }
		printf("\n");
	}

	printf("top 32 bytes @0x0FFFF0: ");
	for(unsigned long i = 0x0FFFF0; i < 0x100000; i++) { hex8(fread(i)); putchar(' '); }
	printf("\n");
	for(;;);
}
