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

patch -p1 -i "$PORT_SRC/qt6.10-private-modules.patch"
patch -p1 -i "$PORT_SRC/fix-reply.patch"

# stack-size: musl gives each thread 128 KiB of stack unless the binary's
# PT_GNU_STACK asks for more, which nheko's worker threads outgrow.
# X11=OFF: the option only adds X11 screen sharing and window roles; the
# session is Wayland and shares screens through the portal (SCREENSHARE_XDP).
export LDFLAGS="$LDFLAGS -Wl,-z,stack-size=1048576"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_SKIP_RPATH=ON \
	-D HUNTER_ENABLED=OFF \
	-D USE_BUNDLED_CPPHTTPLIB=ON \
	-D USE_BUNDLED_BLURHASH=ON \
	-D VOIP=ON \
	-D SCREENSHARE_XDP=ON \
	-D X11=OFF \
	-D MAN=ON \
	-D CI_BUILD=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
