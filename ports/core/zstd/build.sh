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

    sed -i 's/$(shell ls .*|.*(GREP).*)/$(shell find $(LIB_SRCDIR)\/legacy -name "*.c" | grep "v0\[$(ZSTD_LEGACY_SUPPORT)-7\]")/g' lib/libzstd.mk

    export TERM=dumb
    unalias ls 2>/dev/null || true
    unalias grep 2>/dev/null || true

    make
    make PREFIX=/usr DESTDIR=$PKG install
