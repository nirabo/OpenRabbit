# Troubleshooting

## `/dev/ttyUSB0: Input/output error` or the device disappears

The FT232R cable dropped off the USB bus (kernel `error -71`, "unable to
enumerate"). This is a known FT232R issue, made worse by USB hubs. Unplug,
replug (ideally a direct USB port), and re-run. Prefer a CP2102/FT232H adapter
if possible, and keep using `--slow`.

## `Permission denied` / cannot open `/dev/ttyUSB0`

The device is `root:dialout`. Either:

```
sudo usermod -aG dialout $USER   # log out/in
```

or, for the current session, `sudo chmod 666 /dev/ttyUSB0` (this resets when the
cable re-enumerates).

## Hang at `Secondary loader format detected as Dynamic C 9`

Wrong `--ramcr`. The RCM5700 (no external RAM, on-chip SRAM on /CS3) needs:

```
--ramcr 0x43
```

The default `0x45` (external RAM on /CS1) hangs at the pilot handshake.

## `Processor verification sequence failed` / `Status line should be low`

SMODE is not high at reset, so the CPU never enters bootstrap. On the Interface
Board, install the **JP1 pins 1-2** jumper (Program Mode). The programming cable
also drives SMODE from RTS (RTS low = Program Mode), but on this Interface Board
that alone was not reliable.

Note: `tty_setbaud()` must use 8N1 (1 stop bit). Older builds set `CSTOPB`
(2 stop bits), which makes the bootstrap triplets be rejected. Fixed.

## `Flash write failed` / `NAK for subtype 0x43` (plain `openrabbitfu`)

Expected: the precompiled Dynamic C 9 `pilot.bin` flash driver does not support
the RCM5700's S29AL008D. Use the RAM programmer instead:

```
--programmer rcm5700/rcmprog.bin
```

## RAM program produces no output

The SDCC crt0's `STACKSEG = 0x76` (physical 0x76000) is invalid on the RCM5700.
Build with `rcm5700/build.sh`, which patches `STACKSEG` to `0x18`. See
[building.md](building.md). If output is only garbage at first, that is the host
settling on 38400 baud; the readable text follows.

## `Failed to sync with the RCM5700 RAM programmer`

* The programmer takes ~100 ms to set up serial after starting; the host retries
  the sync byte, so a transient failure usually clears on a re-run.
* If it persists, the RAM upload may have failed (check the "sending ... done"
  line) or the target reset into bootstrap failed (try again; the FTDI/DTR
  timing can be flaky).
* `csumR != csumU` while loading the pilot is a transient — just re-run.

## Flashed program does not run

Known open issue; see [status-and-todo.md](status-and-todo.md). Use `--ram` for
development, or Dynamic C / the RFU for persistent images.

## The board no longer runs its original firmware

The flash programmer is destructive and development overwrote the first flash
sector of the test board. Cold-boot mode lives in the CPU ROM and still works,
so re-flash the original image with Dynamic C / the Digi RFU to restore it.
