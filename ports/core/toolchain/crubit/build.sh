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
export RUSTC_BOOTSTRAP=1
cargo build --release --frozen --offline --bin cc_bindings_from_rs \
	--manifest-path cargo/cc_bindings_from_rs/cc_bindings_from_rs/Cargo.toml
install -Dm755 target/release/cc_bindings_from_rs "$PKG/usr/bin/cc_bindings_from_rs"

install -d "$PKG/usr/share/crubit"
cp -r BUILD.gn LICENSE crubit.gni support "$PKG/usr/share/crubit/"
