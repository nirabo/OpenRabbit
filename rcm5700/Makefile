# RCM5700 helper programs
#
# Build with SDCC. See docs/RCM5700/building.md for details and the crt0 patch
# these scripts apply.
#
#   make            # build the non-destructive programs
#   make rcmflash.bin   # destructive self test (erases sector 0)
#   make clean

PROGRAMS = rcmprog ramhello flashid flashscan readtest memtest

.PHONY: all clean

all: $(addsuffix .bin,$(PROGRAMS))

%.bin: %.c build.sh
	./build.sh $*

# Destructive: erases flash sector 0 and programs a test pattern.
rcmflash.bin: rcmflash.c build.sh
	./build.sh rcmflash

clean:
	rm -f *.rel *.ihx *.asm *.lst *.map *.sym *.noi *.lk
	rm -f $(addsuffix .bin,$(PROGRAMS)) rcmflash.bin *-flash.bin
