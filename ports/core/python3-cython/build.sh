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

# A BUILD BACKEND, AND THEREFORE A PORT RATHER THAN A VENDORED WHEEL. Under
# pip's build isolation every consumer would fetch this again from PyPI, and
# meson-python's own chain then reaches the PyPI `ninja` wrapper, whose sdist
# builds CMAKE FROM SOURCE — wrappers that exist for platforms with no system
# cmake, on a tree that has cmake, ninja and meson as ports already.
#
# --no-build-isolation for the same reason, one level down: the only thing an
# isolated environment would install here is setuptools, which is a port.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
