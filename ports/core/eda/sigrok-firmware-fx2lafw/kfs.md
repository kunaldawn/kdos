## About

fx2lafw is open firmware for the logic analysers built on Cypress's FX2 USB microcontroller, most of
them inexpensive boards with no firmware of their own. Such a board enumerates as a bare FX2. When
[libsigrok](#port-libsigrok)'s `fx2lafw` driver finds one, it uploads the matching `fx2lafw-*.fw`
from `/usr/share/sigrok-firmware` into the chip's RAM; without the file the device is listed by a
scan and cannot be opened. The firmware is 8051 code, compiled here from source by the
[sdcc](#port-sdcc) port, whose 8051 back end and device libraries are the whole toolchain it needs.
See
[What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).

## Patches

- `sdcc-syntax.patch` — sdcc from 4.2.3 on accepts an expression after `__at` only in parentheses, as in `__at (0x80+0)`, and an interrupt number only as `__interrupt(N)`. The release spells both the older way, so `fx2lib/include/fx2regs.h`, every interrupt handler in `fx2lib/lib/interrupts/` and in `autovector.h`, and the handlers in `fx2lafw.c` and `include/scope.inc` do not compile with the [sdcc](#port-sdcc) port. The patch rewrites those declarations in the accepted spelling and changes no address or vector.

## Notes

The package holds firmware images for the FX2, not programs for the host, and nothing in it runs on
KDOS itself. Its `configure` only looks for sdcc and `sdas8051`, `sdar`, and `objcopy` or `makebin`,
refuses an sdcc older than 3.4.0, and writes the makefile; there are no switches to set.
