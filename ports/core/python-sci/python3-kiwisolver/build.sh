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

# A RUNTIME DEPENDENCY OF matplotlib, whose constrained layout solves with it.
# A C++ extension built through setuptools with cppy's headers; setup.py
# imports cppy directly, so without the port the build stops before compiling
# anything. --no-build-isolation reaches setuptools, setuptools_scm and cppy.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
