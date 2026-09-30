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

# NLS off: bundled data is English only. The Python module is not built.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--disable-nls \
	--without-python \
	--with-default-dict=/usr/share/cracklib/pw_dict
make
make DESTDIR=$PKG install

# The packed dictionary is what FascistCheck() opens; without it every check
# fails and libpwquality refuses every password. It is packed from the word
# list in the tarball with the tools just built.
util/cracklib-format dicts/cracklib-small > words.sorted
util/cracklib-packer "$PKG/usr/share/cracklib/pw_dict" < words.sorted
