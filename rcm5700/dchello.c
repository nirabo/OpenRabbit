/* dchello.c - Dynamic C test: print over serial A (DC stdio). If this appears
 * on the host, serial A works in Run Mode; if not, the routing is the issue. */
main()
{
   int n = 0;
   while (1) {
      printf("DCHello %d\r\n", n++);
      { unsigned long i; for (i = 0; i < 300000UL; i++) ; }
   }
}
