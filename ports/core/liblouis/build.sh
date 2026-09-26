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

# --without-yaml: libyaml serves only lou_checkyaml, upstream's table test
# driver, so the tool is not built — and its manual page, which the install
# ships regardless, goes with it. The python bindings are a separate setup.py
# that make install does not run; nothing here imports them.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--without-yaml
make
make DESTDIR=$PKG install
rm "$PKG/usr/share/man/man1/lou_checkyaml.1"
