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

# libplacebo generates its OpenGL and EGL loader with `python -m glad` from the
# Khronos registry files this package carries, so the generation reads nothing
# from the network. --no-build-isolation: setuptools is the whole backend, and
# a port.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
