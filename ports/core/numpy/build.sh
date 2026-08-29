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

# NO WHEELS EXIST FOR THIS TARGET. python3 here is 3.14 on musl, and PyPI's
# manylinux wheels are glibc — so this is a from-source build through
# meson-python with its whole chain vendored. openblas is what the linear
# algebra lands on; without it numpy silently falls back to its bundled
# reference kernels and every matrix operation is an order of magnitude slower.

# NO VENDOR BUNDLE. numpy's build backend is `mesonpy` and its only other
# build requirement is Cython; both are PORTS, so --no-build-isolation reaches
# them and nothing is fetched. Vendoring instead means pip resolving the
# backend's own chain, which ends at the PyPI `ninja` wrapper whose sdist
# builds CMake from source.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
