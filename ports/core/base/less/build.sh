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

# PCRE2 IS NAMED AND THEN CHECKED: configure takes musl's POSIX regex first
# when left to choose, and when the named library is missing it falls back to
# no regex at all with only a warning. PCRE2 gives searches Perl syntax.
./configure --prefix=/usr --sysconfdir=/etc --with-regex=pcre2
grep -q '^#define HAVE_PCRE2 1' defines.h \
	|| { echo "less: pcre2 missing" >&2; exit 1; }
make
make DESTDIR=$PKG install
