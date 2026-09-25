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

patch -p1 -i "$PORT_SRC/cstdint.patch"
patch -p1 -i "$PORT_SRC/abseil-nullability.patch"

# --wrap-mode=nofallback keeps meson from answering a missing abseil with the
# subprojects/abseil-cpp.wrap download: the library must link the system
# abseil-cpp, whose pinned options.h is the ABI every other consumer sees.
# -Dneon stays at auto: it resolves from the CPU family alone (on for aarch64,
# unavailable on x86_64), never from a library being present.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nofallback \
	-Dgnustl=disabled \
	-Dinline-sse=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
