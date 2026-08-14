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
MANPATH_DEFAULT="/usr/share/man:/usr/local/share/man"
EOF

./configure
make
make DESTDIR=$PKG install

# The manpath default above covers /usr/share/man; makewhatis builds the
# apropos/whatis index and needs a writable directory to put it in. kpkg
# stages into $PKG, so the index is built on first use rather than here.
