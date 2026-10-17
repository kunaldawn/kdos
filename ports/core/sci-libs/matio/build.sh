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

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-zlib=/usr \
	--with-hdf5=/usr \
	--enable-mat73=yes \
	--enable-extended-sparse=yes \
	--enable-mcos=yes \
	--with-default-file-ver=5
make
makeinfo -I documentation -o documentation/matio.info documentation/matio_user_guide.texi
make DESTDIR=$PKG install
test -s "$PKG"/usr/share/info/matio.info
