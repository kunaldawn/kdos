## About

OpenOCD is an on-chip debugger: through a JTAG or SWD adapter it halts a microcontroller or an SoC,
reads and writes its registers and memory, sets breakpoints, single-steps it and programs its flash,
and it serves all of that to `gdb` as a remote target. Where `gdb` and the tracers debug a process,
OpenOCD debugs a chip with no operating system, which is the way to find out why a board does not
boot far enough to print anything. The port names every adapter driver it builds, so that what it
supports is decided by the recipe and not by what the build root holds.

## Flags

- `--disable-werror` — configure turns warnings into errors whenever GCC warnings are on, which is
  the default; a warning a newer compiler adds would stop the build.
- `--enable-ftdi`, `--enable-stlink`, `--enable-ti-icdi`, `--enable-ulink`,
  `--enable-usb-blaster-2`, `--enable-ft232r`, `--enable-vsllink`, `--enable-xds110`,
  `--enable-cmsis-dap-v2`, `--enable-osbdm`, `--enable-opendous`, `--enable-armjtagew`,
  `--enable-rlink`, `--enable-usbprog`, `--enable-esp-usb-jtag` — The drivers that talk to their
  adapter through [libusb](#port-libusb). configure defaults each adapter to `auto`, which builds
  the driver when its library is found and drops it without a word when not. Named `yes`, a missing
  library is a configure error, so the set of drivers does not depend on what else the build root
  holds.
- `--enable-cmsis-dap`, `--enable-nulink` — The HID drivers, CMSIS-DAP v1 and Nuvoton's Nu-Link,
  through [hidapi](#port-hidapi); `--enable-kitprog`, Cypress KitProg, needs both hidapi and libusb.
- `--enable-usb-blaster`, `--enable-presto`, `--enable-openjtag` — The drivers built on
  [libftdi](#port-libftdi): Altera's USB-Blaster, ASIX Presto and OpenJTAG.
- `--enable-buspirate` — The Bus Pirate driver, which uses a serial port and no library.
- `--enable-jlink` — SEGGER J-Link, through libjaylink.
- `--disable-internal-libjaylink` — The release carries its own copy of libjaylink under
  `src/jtag/drivers/libjaylink`, and configure builds and links that copy unless told not to. With
  the switch, the J-Link driver links the [libjaylink](#port-libjaylink) port.
- `--disable-linuxgpiod` — Leaves out the driver that bit-bangs JTAG and SWD on Linux GPIO lines
  through libgpiod. This release's driver is written against libgpiod's version 1 interface, and the
  [libgpiod](#port-libgpiod) port is version 2, so left to auto-detection the build fails whenever
  libgpiod happens to be installed first. The switch does not appear in `configure --help`, because
  a missing comma in `configure.ac`'s adapter list (`LIBFTDI_USB1_ADAPTERS` before
  `LIBGPIOD_ADAPTERS`) keeps the help text from declaring it, but the variable it sets is the one
  the driver test reads.
- `--enable-jtag_vpi`, `--enable-remote-bitbang` — Two drivers that are off by default and need no
  library: JTAG VPI talks over TCP to a simulated design, and remote bitbang sends ASCII-encoded
  bit-bang requests over a UNIX or TCP socket to another process, which drives the JTAG signals, as
  when the processor being debugged is itself simulated.
- `--with-capstone` — Builds the disassembler behind `arm disassemble`, and OpenOCD's AArch64 one,
  on [capstone](#port-capstone); built without it, the command answers only "capstone disassembly
  framework required". By default configure probes for capstone and goes on without it; given, a
  missing capstone is a configure error.
- `--disable-doxygen-html` — Doxygen's HTML manual of the source code is on by default for the
  `doxygen` make target, which the port does not run.

## Notes

The adapters this port enables and the devices `fs/etc/udev/rules.d/70-kdos-debug.rules` and
`70-kdos-serial.rules` grant to the `dialout` group are the same list, by the USB IDs upstream's
`contrib/60-openocd.rules` gives. Upstream's own rules are not installed: they grant a `plugdev`
group and set the `uaccess` tag, and neither exists on KDOS. An adapter that is in neither KDOS file
can be opened only by root, and enabling a new driver here means adding its IDs there.
