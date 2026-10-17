# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

aclocal -I m4
autoconf
RAWWAVE_PATH=/usr/share/stk/rawwaves/ \
./configure --prefix=/usr --libdir=/usr/lib \
	--enable-shared \
	--disable-static \
	--with-alsa \
	--with-pulse \
	--without-jack

stkflags="$CXXFLAGS -O3 -Wall -I../include -Iinclude -fPIC"
make -C src CFLAGS="$stkflags"
make -C src CFLAGS="$stkflags" DESTDIR=$PKG install

install -d "$PKG/usr/share/stk/rawwaves"
install -m644 rawwaves/*.raw "$PKG/usr/share/stk/rawwaves/"
