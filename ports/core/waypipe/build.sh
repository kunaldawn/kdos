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

# meson hands the build to compile_wrapper.sh, which runs `cargo build
# --frozen` from the build directory; cargo finds the bundle's
# .cargo/config.toml by walking up to the top of the tree. Each feature is
# `auto` upstream and quietly dropped when its library, bindgen or glslc is
# missing, so every one is enabled: video needs FFmpeg's Vulkan hwcontext and
# glslc to compile the colour-conversion shaders; FFmpeg and libgbm are loaded
# with dlopen at run time. build_c stays off: waypipe-c is the superseded C
# implementation. werror is upstream's default and would fail on any new
# compiler warning.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dwerror=false \
	-Db_ndebug=true \
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
