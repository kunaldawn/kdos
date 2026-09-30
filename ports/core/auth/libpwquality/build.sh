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

# No PAM module and no Python binding: the library and pwscore/pwmake are what
# the consumers here (GNOME Disks' passphrase meter) use. NLS off: bundled data
# is English only.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--disable-nls \
	--disable-pam \
	--disable-python-bindings
make
make DESTDIR=$PKG install
