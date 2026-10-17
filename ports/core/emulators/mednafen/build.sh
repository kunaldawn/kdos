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

export CFLAGS="${CFLAGS//-fPIC/}" CXXFLAGS="${CXXFLAGS//-fPIC/}"
./configure \
	--prefix=/usr \
	--disable-nls \
	--disable-rpath \
	--enable-alsa \
	--disable-jack \
	--disable-sdltest \
	--with-external-lzo \
	--with-external-libzstd \
	--with-libflac
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/doc/mednafen"
install -m644 Documentation/*.html Documentation/*.css Documentation/*.png \
	"$PKG/usr/share/doc/mednafen/"
