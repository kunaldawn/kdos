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


# The library's own CFLAGS carry -Werror; a warning a newer compiler adds
# must not stop the build. The Python bindings and the bscalc tool that needs
# them are not built: nothing here imports them.
export CFLAGS="$CFLAGS -Wno-error"
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--without-python3 \
	--without-gtk-doc \
	--without-tools
make
make DESTDIR=$PKG install

rm -rf "$PKG/usr/share/locale"
