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

CFLAGS="$CFLAGS -Wno-error -include libgen.h -include rpmatch.h -DFNM_EXTMATCH=0" \
LDFLAGS="$LDFLAGS -lrpmatch" \
    ./configure --prefix=/usr --program-prefix="eu-" --disable-debuginfod
make
make DESTDIR=$PKG install
