## About

picocom is a minimal serial terminal: it opens a serial port at the speed, parity and flow control
it is given, connects it to the terminal, and offers a few `Ctrl-A` commands to change the settings,
toggle DTR and RTS, send a break or pass a file to an external program such as `sz`. It is the
program for the serial consoles of boards, modems and USB serial adapters, the devices KDOS's
`fs/etc/udev/rules.d/70-kdos-serial.rules` gives the `dialout` group. There is no configure; the
recipe drives upstream's makefile.

## Flags

- `CC="$CC"`, `CFLAGS` — The tree's compiler and flags, given on the make command line, which
  replaces the makefile's own `CFLAGS += -Wall -g`. The makefile's `LDFLAGS ?= -g` leaves the
  exported `LDFLAGS` in force for the link.
- `-DUUCP_LOCK_DIR` — Makes picocom take a UUCP-style lock file for the port in `/run/lock`, which
  `rcS` creates at boot, mode 1777. It is the lock directory [minicom](#port-minicom) is configured
  with as well. Upstream's makefile also defines `USE_FLOCK` in its `CPPFLAGS`, which the command
  line does not replace, so picocom also holds an `flock()` on the device: both kinds of lock, which a comment in
  upstream's makefile says normally not to enable together.
- `-DHIGH_BAUD` — Accepts the standard speeds above 115200, up to 4000000. Upstream's makefile
  defines it too; the recipe states it.
- `-DNO_CUSTOM_BAUD` — Leaves out arbitrary baud rates. picocom sets them through Linux's `struct
  termios2` and the `TCSETS2` ioctl, and its `termios2.c` expects the termios fields to be named
  `c_ispeed` and `c_ospeed`; musl names them `__c_ispeed` and `__c_ospeed`, and the file does not
  compile. Every speed that is one of the standard constants still works.

## Patches

- `pinfo-prototype.patch` — Declares `int pinfo(const char *format, ...);` near the top of `picocom.c`. The function is defined after its first use, which is inside the `UUCP_LOCK_DIR` block: upstream's makefile builds with `USE_FLOCK` alone and never compiles that call, and with `UUCP_LOCK_DIR` set GCC reports both the implicit declaration and the later definition, as conflicting types.
