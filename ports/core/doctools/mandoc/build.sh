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

cat > configure.local <<EOF
CFLAGS="$CFLAGS"
LDFLAGS="$LDFLAGS"
PREFIX="/usr"
BINDIR="/usr/bin"
SBINDIR="/usr/sbin"
MANDIR="/usr/share/man"
INCLUDEDIR="/usr/include"
LIBDIR="/usr/lib"
UTF8_LOCALE="C.UTF-8"
BINM_MAN="man"
BINM_APROPOS="apropos"
BINM_WHATIS="whatis"
BINM_MAKEWHATIS="makewhatis"
BINM_PAGER="less"
MANPATH_DEFAULT="/usr/share/man:/usr/local/share/man"
EOF

./configure
make
make DESTDIR=$PKG install
