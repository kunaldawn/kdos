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


# PURE PYTHON. setup.py compiles every module with Cython only under
# --native, which pip is never asked for here, so the package is the modules
# as written and `import RNS` behaves the same with or without a C compiler
# on the machine. rnsd, rnstatus, rnodeconf, rnsh, rngit and the other
# utilities are entry points of this one package.
#
# rnodeconf fetches firmware from GitHub unless it is told a version and
# `--nocheck`. The images it would fetch are the rnode-firmware port, whose
# `rnodeconf-local` runs it that way against them.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
