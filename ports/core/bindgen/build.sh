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

export LIBCLANG_PATH=/usr/lib/
export LIBCLANG_STATIC_PATH=/usr/lib/

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export RUSTFLAGS="-C link-arg=-lzstd -C target-feature=-crt-static"
cargo build --release --frozen --offline -p bindgen-cli --no-default-features --features logging,runtime
install -Dm755 target/release/bindgen $PKG/usr/bin/
