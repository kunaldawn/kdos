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

for p in "$_pfx"-[0-9][0-9][0-9]; do
	patch -p0 -i "$p"
done

./configure --prefix=/usr \
	--with-curses --with-shared-termcap-library \
	--disable-static --disable-install-examples \
	bash_cv_termcap_lib=libncursesw
_xld='SHLIB_XLDFLAGS=-Wl,-soname,`basename $@ $(SHLIB_MINOR)`'
make "$_xld"
make DESTDIR=$PKG "$_xld" install
test -z "$(readelf -d "$PKG"/usr/lib/libreadline.so.*.* | grep -i 'rpath\|runpath')"
