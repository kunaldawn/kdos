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

export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr \
	-Csetup-args=-Dcpu-baseline=none \
	-Csetup-args=-Dblas=openblas \
	-Csetup-args=-Dlapack=openblas \
	-Csetup-args=-Dallow-noblas=false \
	.
find "$PKG"/usr/lib/python3*/site-packages/numpy -type d -name tests -prune -exec rm -rf {} +
