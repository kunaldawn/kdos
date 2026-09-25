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

mkdir -p build && cd build
# No X11 display. The SDL3 phosphor scope draws on Wayland, and pulse input
# (-t hw, the default with no file) records from the session's pipewire-pulse.
# Both options exist only when find_package finds their library, so an ON here
# with the library missing is a link that fails rather than a feature that
# quietly vanishes.
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DX11_SUPPORT=OFF -DPULSE_AUDIO_SUPPORT=ON -DSDL3_SCOPE=ON
make
make DESTDIR=$PKG install
install -Dm644 ../gen-ng.1 -t $PKG/usr/share/man/man1
