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

./configure --prefix=/usr \
	--sysconfdir=/etc \
	--enable-perl-xs \
	--enable-nls \
	PERL_EXT_CFLAGS="$CFLAGS" \
	PERL_EXT_LDFLAGS="$LDFLAGS"
grep -q '^#define ENABLE_NLS 1' config.h || { echo "texinfo: NLS not configured" >&2; exit 1; }
make
make DESTDIR=$PKG install
