// Probe which memory bank exposes the RCM5700's on-chip SRAM via /CS3.
#include <stdint.h>
#include <stdio.h>
#include "r2k.h"
#include "targetconfigurations.h"

__sfr __at(0x18) MECR;
__sfr __at(0x1E) DATASEGL;
__sfr __at(0x1F) DATASEGH;

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
static unsigned long rtclock(void){ unsigned long a,b; do{ RTC0R=0; a=RTC0R|((unsigned long)RTC1R<<8)|((unsigned long)RTC2R<<16)|((unsigned long)RTC3R<<24); b=RTC0R|((unsigned long)RTC1R<<8)|((unsigned long)RTC2R<<16)|((unsigned long)RTC3R<<24);}while(a!=b); return b; }

static unsigned char test_window(unsigned long page)
{
	DATASEGL = (unsigned char)(page & 0xFF);
	DATASEGH = (unsigned char)((page >> 8) & 0x0F);
	volatile unsigned char *p = (volatile unsigned char *)0x8000;
	p[0] = 0x5A;
	p[1] = 0xA5;
	return (p[0] == 0x5A && p[1] == 0xA5);
}

void main(void)
{
	GOCR = 0x30; PCFR = 0x40; TAT4R = SERIAL_DIVIDER_38400 - 1; TACSR = 0x01; SACR = 0x00;
	{ unsigned long c=rtclock(); while(rtclock()-c < 32*100); }

	MECR = 0x20;	// 512 KB banks

	// MB0 is /CS3 (we run from it). Test a non-aliasing page in it: 0x190000.
	MB3CR = 0xC3;
	printf("MB3 (/CS3) @0x190000: %s\n", test_window(0x190) ? "OK" : "FAIL");

	// MB1 = /CS3
	MB1CR = 0x43;
	printf("MB1 (/CS3) @0x090000: %s\n", test_window(0x090) ? "OK" : "FAIL");

	// MB2 = /CS3
	MB2CR = 0xC3;
	printf("MB2 (/CS3) @0x110000: %s\n", test_window(0x110) ? "OK" : "FAIL");

	for(;;);
}
