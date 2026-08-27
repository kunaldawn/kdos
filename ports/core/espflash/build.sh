#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# ESPFLASH RATHER THAN espflash's REFERENCE COUSIN esptool, and the reason is
# the dependency chain rather than the feature set. Espressif's own esptool is
# python and reaches `cryptography`, which is cffi plus a rust extension —
# seven new python ports and a compiled crypto stack, for a program that writes
# a binary down a serial line. espflash is Espressif's own Rust flasher, one
# static binary, and it drives the same ROM bootloader on the same chips.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline --bin espflash
install -Dm755 target/release/espflash $PKG/usr/bin/espflash
