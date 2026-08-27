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

# NO VENDOR BUNDLE, because there is nothing to vendor: fontTools is
# zero-dependency by design and everything in its requirements.txt is an
# OPTIONAL extra — that file reaches scipy, thirty megabytes and a Fortran
# compiler, for a font inspector.
#
# --no-build-isolation for the same reason: the only thing pip's isolated
# build environment would fetch is setuptools, which is an installed port.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
