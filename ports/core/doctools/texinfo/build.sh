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

# --enable-perl-xs skips the "can we build XS?" probe and builds the C
# modules, so a toolchain problem fails the build instead of leaving makeinfo
# on the pure-Perl parser, which is far slower on a large manual.
#
# --enable-nls only asks: musl's libc has no GNU gettext, and without the
# libintl port configure turns translations off and succeeds. config.h is the
# answer.
./configure --prefix=/usr \
	--enable-perl-xs \
	--enable-nls
grep -q '^#define ENABLE_NLS 1' config.h || { echo "texinfo: NLS not configured" >&2; exit 1; }
make
make DESTDIR=$PKG install
