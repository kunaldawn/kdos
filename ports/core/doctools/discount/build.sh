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

patch -p1 -i "$PORT_SRC/install-data-mode.patch"

# configure.sh is discount's own script, not autoconf: the default is a static
# library, and consumers such as Okular's Markdown generator find the shared
# one through the .pc file --pkg-config installs. --container keeps it from
# running ldconfig against the build root.
./configure.sh --prefix=/usr --libdir=/usr/lib --mandir=/usr/share/man \
	--shared \
	--pkg-config \
	--container
make
make DESTDIR=$PKG install install.man
