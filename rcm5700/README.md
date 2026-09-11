# rcm5700 — helper programs

Source for the RAM-resident flash programmer and the diagnostic programs used
to bring up the Digi RCM5700W (Rabbit 5000) on Linux.

* `rcmprog.c` / `rcmprog.bin` — the RAM flash programmer (the deliverable).
* `rcmflash.c` — on-target flash self test (destructive).
* `flashid.c`, `flashscan.c`, `readtest.c`, `memtest.c`, `ramhello.c` — diagnostics.
* `tiny.s` — 63-byte hand-written assembly boot test.
* `build.sh` / `buildflash.sh` — SDCC build helpers (apply the crt0 patch).

Build with `make` (requires SDCC + objcopy).

Full documentation, hardware notes, the serial protocol and the current status
are in [`../docs/RCM5700/`](../docs/RCM5700/README.md).
