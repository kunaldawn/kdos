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
#
# THE 2.13 LINE, NOT 3.x, and the reason is the backend rather than the API.
# pybind11 3 builds through `scikit_build_core`, which under
# --no-build-isolation would have to be a port too, along with its own chain —
# for a header-only library. matplotlib asks for `>=2.13.2,!=2.13.3` and 2.13.6
# builds with setuptools, which is already here.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
