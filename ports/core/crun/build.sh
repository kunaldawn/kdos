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

# blake3 is the embedded copy: left at auto, configure takes a libblake3 found
# by pkg-config instead. CRIU (checkpoint and restore) is off because criu is
# not a port; left at auto it is taken whenever a libcriu happens to be there.
./autogen.sh
./configure \
	--prefix=/usr \
	--disable-systemd \
	--enable-embedded-blake3 \
	--disable-criu \
	--disable-shared \
	--enable-static
make
make DESTDIR=$PKG install
