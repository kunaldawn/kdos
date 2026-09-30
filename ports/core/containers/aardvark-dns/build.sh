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

# musl has no close_range() wrapper, so the libc crate declares it for glibc
# targets only and main.rs does not compile here. The patch makes the same
# call through syscall(SYS_close_range), which both C libraries have.
patch -p1 -i "$PORT_SRC/musl-close-range.patch"
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
cargo build --release --frozen --offline
install -Dm755 target/release/aardvark-dns "$PKG/usr/libexec/podman/aardvark-dns"
