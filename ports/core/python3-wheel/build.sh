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

# QEMU's mkvenv installs qemu.qmp and wheel in ONE pip invocation, so a missing
# `wheel` fails the vendored qemu.qmp wheel with it and configure reports the
# qmp package rather than this one.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
python3 -m pip install . --ignore-installed --no-deps --find-links=vendor --no-index --root="$PKG" --prefix=/usr
