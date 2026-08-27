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

# NO VENDOR BUNDLE AND NO BUILD ISOLATION. sympy is pure python with one
# runtime dependency, mpmath, which is a port; the only thing an isolated
# build environment would fetch is setuptools, which is also a port. With
# isolation on and nothing vendored, pip fails on
# `Could not find a version that satisfies the requirement setuptools`.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
