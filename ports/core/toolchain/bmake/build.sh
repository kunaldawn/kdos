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

env ENV.directive-export=QUOTING_STYLE=literal \
    ENV.directive-export-gmake=QUOTING_STYLE=literal \
    "ENV.export=QUOTING_STYLE=literal FILTER_CMD=\"grep -Ev '^(FILTER_CMD|MAKEFLAGS|MALLOC_.*|PATH|PWD|QUOTING_STYLE|SHLVL|_|&)='\"" \
    ENV.export-all=QUOTING_STYLE=literal \
    ENV.unexport=QUOTING_STYLE=literal \
    BROKEN_TESTS=cmd-interrupt sh ./boot-strap --prefix=/usr --install-destdir=$PKG op=install
