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

mkdir .kdos-vendor
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz -C .kdos-vendor \
	--exclude=vendor/axoasset

export RUSTFLAGS="-C target-feature=-crt-static"

cargo build --release --frozen --offline --config .kdos-vendor/.cargo/config.toml \
	--package atuin --no-default-features --features client,clipboard,pty-proxy
readelf -d target/release/atuin | grep -q 'NEEDED.*\[libsqlite3\.so\.0\]'
readelf -d target/release/atuin | grep -q 'NEEDED.*\[libzstd\.so\.1\]'

install -Dm755 target/release/atuin $PKG/usr/bin/atuin
