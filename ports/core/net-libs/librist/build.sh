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

meson setup build --prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dtest=false \
	-Dbuilt_tools=true \
	-Dbuiltin_cjson=false \
	-Dbuiltin_lz4=false \
	-Dbuiltin_mbedtls=false \
	-Dfallback_builtin=false \
	-Duse_mbedtls=true \
	-Duse_nettle=false \
	-Duse_gnutls=false \
	-Dallow_insecure_iv_fallback=false \
	-Dallow_obj_filter=false \
	-Dstatic_analyze=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

test -f "$PKG/usr/lib/librist.so"
test -x "$PKG/usr/bin/ristsender"
