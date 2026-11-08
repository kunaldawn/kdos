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

LIBS="$PORT_SRC/../../libs"

cmake -S "$PORT_SRC" -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" \
	-o kdos-ports-helper "$PORT_SRC"/helper/kdos-ports-helper.c "$LIBS"/libkbase/*.c $LDFLAGS
install -Dm755 kdos-ports-helper "$PKG/usr/libexec/kdos-discover/kdos-ports-helper"
install -Dm644 "$PORT_SRC"/helper/io.kdos.ports.policy "$PKG/usr/share/polkit-1/actions/io.kdos.ports.policy"
