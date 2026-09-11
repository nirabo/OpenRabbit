// Read the first bytes of the RCM5700 flash at several bank mappings.
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
{ WDTTR=0x51; WDTTR=0x54; GCSR=0x08; GCDR=CLOCK_DOUBLER; return 0; }

int putchar(int c){ if(c=='\n') putchar('\r'); while(SASR&0x04); SADR=c; return c; }
static void hex8(unsigned char v){ const char*h="0123456789ABCDEF"; putchar(h[v>>4]); putchar(h[v&15]); }
static unsigned long rtclock(void){ unsigned long a,b; do{ RTC0R=0; a=RTC0R|((unsigned long)RTC1R<<8)|((unsigned long)RTC2R<<16)|((unsigned long)RTC3R<<24); b=RTC0R|((unsigned long)RTC1R<<8)|((unsigned long)RTC2R<<16)|((unsigned long)RTC3R<<24);}while(a!=b); return b; }

static void set_page(unsigned long page){ DATASEGL=(unsigned char)(page&0xFF); DATASEGH=(unsigned char)((page>>8)&0x0F); }

static void dump(const char *label, unsigned long page)
{
	set_page(page);
	printf("%s: ", label);
	for(unsigned int i=0;i<16;i++){ hex8(WIN[i]); putchar(' '); }
	printf("\n");
}

void main(void)
{
	GOCR=0x30; PCFR=0x40; TAT4R=SERIAL_DIVIDER_38400-1; TACSR=0x01; SACR=0x00;
	{ unsigned long c=rtclock(); while(rtclock()-c < 32*100); }

	MECR=0x20;
	MB1CR=0x00; MB2CR=0x00; MB3CR=0x00;

	printf("\nRCM5700 flash read test\n");
	dump("MB2 phys0x100000 (off 0x00000)", 0x100);
	dump("MB1 phys0x080000 (off 0x80000?)", 0x080);
	dump("MB3 phys0x180000 (off 0x80000?)", 0x180);

	// JEDEC ID via MB2
	set_page(0x100);
	WIN[0xAAA]=0xAA; WIN[0x555]=0x55; WIN[0xAAA]=0x90;
	printf("ID: "); hex8(WIN[0x00]); putchar(' '); hex8(WIN[0x02]); printf("\n");
	WIN[0xAAA]=0xF0;
	for(;;);
}
