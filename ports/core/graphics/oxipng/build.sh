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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export RUSTFLAGS="-C target-feature=-crt-static"

cargo build --release --frozen --offline --bin oxipng
install -Dm755 target/release/oxipng $PKG/usr/bin/oxipng

patch -p1 -i "$PORT_SRC/source-date-epoch.patch"
cargo run --frozen --offline --manifest-path xtask/Cargo.toml -- mangen
install -Dm644 target/xtask/mangen/manpages/oxipng.1 -t "$PKG/usr/share/man/man1"
