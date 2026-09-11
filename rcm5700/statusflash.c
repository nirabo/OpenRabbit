/* statusflash.c - Dynamic C test: toggle the STATUS pin (GOCR) so OpenRabbit
 * can observe it via the cable's DSR line. If this runs from flash in Run Mode,
 * STATUS will toggle; if the DC BIOS/program does not run, it will not. */
main()
{
   while (1) {
      WrPortI(GOCR, NULL, 0x30);   /* STATUS high */
      { unsigned int i; for (i = 0; i < 60000U; i++) ; }
      WrPortI(GOCR, NULL, 0x20);   /* STATUS low  */
      { unsigned int i; for (i = 0; i < 60000U; i++) ; }
   }
}
