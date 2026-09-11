# Ecosystem and future of the Rabbit platform

Summary of a web check (September 2026) on Linux support for Rabbit boards and
whether the platform is still maintained. Sources are listed at the bottom.

## Linux support: yes, and improving

* **SDCC has Rabbit support, including the Rabbit 5000.** The **SDCC 4.6.0**
  release (2026-06-22) added:
  * the **`r4k` port for Rabbit 4000**,
  * **experimental `r5k` and `r6k` ports for Rabbit 5000 and 6000** - the
    RCM5700's CPU,
  * **support for the Dynamic C calling convention** in the z80-related ports,
  * **`__far` support in the Rabbit ports for a 1 MB address space** (the
    RCM5700's flash/RAM layout).
* There is an active effort to build an **SDCC-based replacement for Dynamic C**,
  led by Philipp Klaus Krause (spth): a plan at
  <http://www.colecovision.eu/stuff/Rabbitplan.pdf> and the Digi forum thread
  <https://forums.digi.com/t/creating-an-sdcc-based-development-environment-to-replace-dynamic-c/36307>
  (April 2025).
* Other Linux paths:
  * **OpenRabbit** (this project) - RAM load, flash programming, see
    [README.md](README.md).
  * **Dynamic C under Wine** - how we build real RCM5700 images; see
    [dynamic-c-wine.md](dynamic-c-wine.md). Forum:
    <https://forums.digi.com/t/dynamic-c-from-linux-command-line-through-wine/30383>.
  * **Softools** - a legacy commercial Rabbit C compiler, still available with
    very limited support; targets Rabbit 2000/3000 only, ISO C90 (+ bits of C99).

## Maintenance in 2026: the platform is end-of-life

* **Digi EOL'd *all* Rabbit products in late 2024** (PCN 240826 / EOL-230410).
  Customers had until the **end of January 2025** for a last-time buy, with
  final shipments scheduled to complete by the end of 2025. The LTB covers
  Rabbit 2000/3000/4000/6000 ICs.
* Dynamic C's libraries were open-sourced (`digidotcom/DCRabbit_9`,
  `digidotcom/DCRabbit_10`) but are **frozen** - no active Digi development.
  The last releases are from ~2016-2020.
* In the forum thread above, Tom Collins (the sole Digi engineer supporting
  Rabbit since 2015) writes: *"It certainly feels like the end of an era."*
* The **SDCC `r5k`/`r6k` ports are the one live, current effort**, but they are
  explicitly **experimental** and, as the thread notes, likely "too late to
  save the Rabbits." The Rabbit 5000/6000 also contain Digi IP, so third-party
  silicon is unlikely.

## What this means for OpenRabbit

* The platform is effectively "retro", but keeping existing boards alive is
  exactly the use case that remains viable.
* The most promising future direction for the flash-boot problem is to try
  **SDCC 4.6.0's `r5k` port** (`-mr5k`) with its `__far` 1 MB support. Our
  builds used the older `r2k` port (`-mr2k`); a native r5k port may handle the
  Rabbit 5000 memory map more directly and could avoid the hand-written
  BIOS-preamble approach we were forced into.
* `DCRabbit_10` remains the authoritative reference for the boot contract and
  the DC libraries, even though it is frozen.

## Sources

* SDCC 4.6.0 release notes - <https://sourceforge.net/p/sdcc/news/2026/06/sdcc-460-released/>
* SDCC project - <https://sdcc.sourceforge.net/>
* "Creating an SDCC-based development environment to replace Dynamic C?" -
  <https://forums.digi.com/t/creating-an-sdcc-based-development-environment-to-replace-dynamic-c/36307>
* "Dynamic C From Linux Command Line Through Wine" -
  <https://forums.digi.com/t/dynamic-c-from-linux-command-line-through-wine/30383>
* Digi EOL notifications (PCN 240826 / EOL-230410) via Digi-Key/Mouser/Anglia.
* `digidotcom/DCRabbit_9`, `digidotcom/DCRabbit_10` on GitHub.
