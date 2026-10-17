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

case "$(uname -m)" in
	aarch64) _lgpage=16 ;;
	*) _lgpage=12 ;;
esac
export CFLAGS="${CFLAGS/-O2/-O3} -g0" CXXFLAGS="${CXXFLAGS/-O2/-O3} -g0"
./configure --prefix=/usr --libdir=/usr/lib --sysconfdir=/etc \
	--localstatedir=/var \
	--disable-static \
	--disable-doc \
	--enable-xmalloc \
	--with-lg-page=$_lgpage \
	--with-lg-hugepage=21
make
make DESTDIR=$PKG install
install -Dm644 doc/jemalloc.3 "$PKG/usr/share/man/man3/jemalloc.3"

test -f "$PKG/usr/lib/libjemalloc.so"
