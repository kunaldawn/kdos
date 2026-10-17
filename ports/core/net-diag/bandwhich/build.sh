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
mkdir -p gen
export BANDWHICH_GEN_DIR="$SRC/gen"
export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline
install -Dm755 target/release/bandwhich $PKG/usr/bin/bandwhich
install -Dm644 gen/bandwhich.1 -t "$PKG/usr/share/man/man1"
