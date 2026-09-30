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

# Alpine's musl patches, in Alpine's order: the includes musl does not pull in
# transitively, the format-string and gcc 10 common-symbol errors, and the GNU
# basename() fsck and fscklog assume.
for p in format-security musl-fix-includes jfsutils-include-sysmacros \
	missing-stdinth gcc-10 fflush-stdout gnu-basename; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

./configure --prefix=/usr --sbindir=/usr/sbin --mandir=/usr/share/man
make
make -j1 DESTDIR=$PKG install
