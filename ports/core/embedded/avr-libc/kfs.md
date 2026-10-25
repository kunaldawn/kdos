## About

AVR-LibC is the C library for Microchip's 8-bit AVR microcontrollers, the chips of the classic
Arduino boards among them: the headers that name every register of every device, the start-up code,
`libc.a` and `libm.a` for each core family, and the `<avr/…>` and `<util/…>` helpers for EEPROM,
program-memory strings, interrupts, sleep modes and delays. With it [gcc-avr](#port-gcc-avr)
compiles and links firmware. Like the picolibc ports for ARM and RISC-V, it is built with the cross
compiler: gcc-avr is built first, without headers, so that its `libgcc` exists before
any C library does.

## Flags

- `unset CC CXX CFLAGS CXXFLAGS LDFLAGS CPPFLAGS` — Clears the tree's native compiler and flags
  before configure. The phase environment names the host `gcc` and host flags; avr-libc's configure
  checks that the compiler it is given targets AVR and stops at "Wrong C compiler found; check the
  PATH!" otherwise. With the variables gone, `--host=avr` picks `avr-gcc`.
- `--host=avr`, `--build` — Make this a cross build for AVR. configure stops when the build and host
  systems are the same or the host is not an AVR. `--build` is given as `./config.guess` reports
  it, the form configure's own error message suggests.
- `--disable-doc` — The manual is off by default; the switch states it. Building it would need
  doxygen and fig2dev, and its man pages are made by the same pass.

## Patches

- `ccp-write-spm-avrtiny.patch` — `ccp_write_spm()` in `libc/misc/ccp_write_spm.S` writes the configuration change protection signature to the `CCP` register with `sts`. On the reduced AVRtiny core (`__AVR_TINY__`) the short `sts` reaches only data addresses `0x40` to `0xBF`, and `CCP` is at I/O address `0x3C`, so the patch writes it there with `out` and keeps `sts` for every other core.

## Notes

`avr-man` is deleted from the package. It runs `man` against the directory the manual would be
installed in, and with the manual not built that directory does not exist.
