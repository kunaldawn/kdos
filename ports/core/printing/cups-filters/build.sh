# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export CFLAGS="$CFLAGS -std=gnu17"
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--enable-universal-cups-filter --disable-individual-cups-filters \
	--enable-imagefilters --enable-poppler --enable-ghostscript \
	--disable-mutool --enable-foomatic --enable-driverless --disable-nls \
	--with-shell=/bin/bash
make
make -j1 DESTDIR=$PKG install
