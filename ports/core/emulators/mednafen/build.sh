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

# JACK is off: audio is ALSA, which this image routes to PipeWire. LZO and
# zstd are the system ports rather than the bundled copies, so a fix to either
# reaches this program. NLS is off: bundled data is English only. The SDL test
# program is not run, because configure would execute it on a display the
# build does not have.
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

# The HTML manual is the only documentation upstream ships, and the only
# place each system's settings are listed.
install -d "$PKG/usr/share/doc/mednafen"
install -m644 Documentation/*.html Documentation/*.css Documentation/*.png \
	"$PKG/usr/share/doc/mednafen/"
