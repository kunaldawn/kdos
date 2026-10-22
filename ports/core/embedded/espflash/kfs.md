## About

espflash is the esp-rs project's flasher for Espressif chips, written in Rust: one program that
drives the ROM bootloader of an ESP32-family chip over its USB serial port to write an application
ELF or a binary into the chip's flash, read and erase flash, and convert partition tables, and its
`monitor` shows the device's serial output. It reaches the same bootloader as Espressif's Python
reference tool, [python3-esptool](#port-python3-esptool), without its Python dependencies. The
flasher stubs it uploads into the chip's RAM to speed up writing are code for the chip, carried in
the source as published; see [What is built from
source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).

## Flags

- `--bin espflash` — Builds only the `espflash` program from upstream's workspace, which also holds
  `cargo-espflash`, the same tool as a cargo subcommand for flashing a Rust project, and `xtask`,
  upstream's own maintenance tasks.
- `CARGO_NET_OFFLINE=true` — Cargo's environment form of `net.offline`, the same as `--offline` on
  the command line.

## Notes

This release supports the ESP32 and its C, H, P and S series, and not the ESP8266, which
python3-esptool flashes. The serial port is opened through the `serialport` crate with its default
features off, so no libudev is linked. The `kdos` user reaches USB serial adapters through the
`dialout` group, which `fs/etc/udev/rules.d/70-kdos-serial.rules` gives each one.
