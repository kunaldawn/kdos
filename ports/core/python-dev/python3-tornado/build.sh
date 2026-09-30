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

# TORNADO_EXTENSION=1 makes the C speedups (websocket masking) a build
# requirement: without it a compiler failure is silently a slower module.
export TORNADO_EXTENSION=1
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
test -n "$(find "$site/tornado" -maxdepth 1 -name 'speedups*.so')"
