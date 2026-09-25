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

# slsh's zlib and png modules link the zlib and libpng ports. --with-z and
# --with-png still fall back silently when the library is not found, so the
# module list configure wrote is checked: a missing one stops here instead of
# shipping an slsh without it. pcre1 and oniguruma are not ports; --without-x
# stops only an X probe the socket module would read link flags from.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--with-readline=gnu \
	--without-x \
	--without-pcre \
	--without-onig \
	--with-png \
	--with-z

for mod in zlib-module.so png-module.so; do
	grep -A4 '^MODULES = ' modules/Makefile | grep -qw "$mod" || {
		echo "slang: $mod not configured" >&2
		exit 1
	}
done
make -j1
make DESTDIR=$PKG install
