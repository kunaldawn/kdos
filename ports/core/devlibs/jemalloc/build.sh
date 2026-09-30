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

# The page size is named rather than probed, so the library does not depend
# on the machine it was built on: 4 KiB on x86_64, and 64 KiB on arm64, whose
# kernels come in 4, 16 and 64 KiB pages and which a smaller setting would
# refuse to run under. The manual page is shipped prebuilt in the tarball;
# --disable-doc keeps the build from regenerating it and the HTML through
# xsltproc.
case "$(uname -m)" in
	aarch64) _lgpage=16 ;;
	*) _lgpage=12 ;;
esac
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
