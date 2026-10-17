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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--disable-nls \
	--without-python \
	--with-default-dict=/usr/share/cracklib/pw_dict
make
make DESTDIR=$PKG install

util/cracklib-format dicts/cracklib-small > words.sorted
util/cracklib-packer "$PKG/usr/share/cracklib/pw_dict" < words.sorted
