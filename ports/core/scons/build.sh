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

# A BUILD TOOL, and gpsd is the only consumer here. gpsd has no Makefile and no
# meson.build; scons IS its build system, so this is the price of a GPS daemon
# rather than a preference between build tools.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
pip3 install --no-deps --no-index --find-links=vendor --root=$PKG --prefix=/usr .
