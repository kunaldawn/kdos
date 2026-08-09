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

mkdir -p src .cargo
cp "$PORT_SRC/main.rs" src/
cp "$PORT_SRC/Cargo.toml" "$PORT_SRC/Cargo.lock" .
cp "$PORT_SRC/cargo-config.toml" .cargo/config.toml
tar xf "$PORT_SRC/${name}-vendor-${version}.tar.xz"
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline
install -Dm755 target/release/kdos-theme-helper "$PKG/usr/bin/kdos-theme-helper"
