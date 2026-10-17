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
export LIBCLANG_PATH=/usr/lib

meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dwerror=false \
	-Dbuild_rs=true \
	-Dbuild_c=false \
	-Dtests=false \
	-Dman-pages=enabled \
	-Dwith_video=enabled \
	-Dwith_dmabuf=enabled \
	-Dwith_lz4=enabled \
	-Dwith_zstd=enabled \
	-Dwith_gbm=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
