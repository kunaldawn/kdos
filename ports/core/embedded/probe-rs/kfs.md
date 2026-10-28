## About

probe-rs flashes and debugs ARM Cortex-M, RISC-V and Xtensa microcontrollers through a debug probe:
CMSIS-DAP (DAPLink), ST-Link, J-Link, FTDI probes, the ESP32's built-in USB-JTAG, WCH-Link and the
Black Magic Probe. Beside flashing it reads [RTT]{.idx} (Real-Time Transfer), a channel through a
ring buffer in the target's own RAM over the debug probe, so a running chip can print without a UART
or a spare pin; it also offers a GDB server and a Debug Adapter Protocol server for editors. The
port installs three programs from the `probe-rs-tools` crate: `probe-rs` itself, and the cargo
subcommands `cargo-flash`, which programs a compiled Rust project or any ELF into the chip, and
`cargo-embed`, which adds an RTT terminal and a GDB server.

## Flags

- `-p probe-rs-tools` — Builds only the `probe-rs-tools` package of upstream's workspace, the one
  that holds the three programs. The other members are the libraries those programs are built from
  and upstream's own tools (`target-gen`, `smoke-tester`, `xtask`).
- `CARGO_NET_OFFLINE=true` — Cargo's environment form of `net.offline`, the same as `--offline` on
  the command line.

## Notes

- **The chip descriptions are compiled in.** The `probe-rs` library's default `builtin-targets`
  feature builds the target descriptions under `probe-rs/targets` into the program, and the
  `probe-rs-espressif` crate builds in the Espressif ones under its own `targets`, each with its
  memory map and the flash algorithms that run on the chip, so nothing has to be downloaded for a
  supported part. A part outside that set needs a chip description file, which upstream's
  `target-gen` makes from a CMSIS-Pack, given with `--chip-description-path`. The flash algorithms
  are code for the target, carried as upstream publishes them; see
  [What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).
- **No C library is linked for USB.** Probes are reached through `nusb` and through `hidapi` with
  its `linux-native-basic-udev` back end, both of which talk to `/dev/bus/usb` and `/dev/hidraw*`
  directly, so the program NEEDs only the C library and `libgcc_s`. Access to the probes as the
  desktop user comes from `fs/etc/udev/rules.d/70-kdos-debug.rules`.
