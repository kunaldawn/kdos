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
export LIBCLANG_PATH=/usr/lib

cargo build --release --frozen --offline --bin hurl --bin hurlfmt
install -Dm755 target/release/hurl    $PKG/usr/bin/hurl
install -Dm755 target/release/hurlfmt $PKG/usr/bin/hurlfmt
install -Dm644 docs/manual/hurl.1 docs/manual/hurlfmt.1 -t "$PKG/usr/share/man/man1"
