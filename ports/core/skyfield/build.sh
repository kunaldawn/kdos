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

# NO VENDOR BUNDLE: the build needs only setuptools, and every runtime
# dependency — numpy, jplephem, sgp4, certifi — is a port. Vendoring them
# instead means pip resolving numpy's sdist, which reaches the PyPI `ninja`
# wrapper and builds CMake from source.
#
# THE EPHEMERIS IS NOT SHIPPED. Skyfield computes positions from a JPL kernel
# and DE440s is 32 MB; `load('de440s.bsp')` downloads it, which is the one
# thing this machine will not do. Point it at a file that is already here.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
