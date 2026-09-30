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

# A RUNTIME DEPENDENCY OF matplotlib: every contour and contourf call is this.
# A pybind11 extension built through meson-python, the numpy chain.
# --no-build-isolation: meson-python, meson, ninja and pybind11 are all ports,
# and an isolated environment would fetch them over a network the build lacks.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
