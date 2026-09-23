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

# mandoc has no autoconf: configure is a hand-written shell script that reads
# ./configure.local. It is the whole build configuration, so it is written
# here rather than passed as flags.
#
# Every variable below is one ./configure actually reads — checked against the
# script rather than guessed, because it silently ignores anything else.
# BINM_* install mandoc under the names people actually type; nothing else on
# KDOS provides man/apropos/whatis, so there is no conflict to arbitrate.
cat > configure.local <<'EOF'
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

# BINM_MAKEWHATIS above is the indexer, and the index it writes is a
# `mandoc.db` inside each manual root. kpkg stages into $PKG, where a root
# holds only this package's own pages, so the index is stamped into the image
# once every package is installed — by script/06_packaging/00_whatis.sh.
# Without that step apropos and whatis reach mansearch(), find no database and
# print nothing; only `man` falls back to walking the filesystem.
