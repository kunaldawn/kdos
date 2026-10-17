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

export CARGO_HOME="$SRC_ROOT/.cargo"
export CARGO_NET_OFFLINE=true
export RUSTFLAGS="-C target-feature=-crt-static"
export ZSTD_SYS_USE_PKG_CONFIG=1
export LIBSQLITE3_SYS_USE_PKG_CONFIG=1
cargo build --release --frozen --offline
install -Dm755 target/release/rga             $PKG/usr/bin/rga
install -Dm755 target/release/rga-preproc     $PKG/usr/bin/rga-preproc

install -Dm755 target/release/rga-fzf         $PKG/usr/bin/rga-fzf
install -Dm755 target/release/rga-fzf-open    $PKG/usr/bin/rga-fzf-open
