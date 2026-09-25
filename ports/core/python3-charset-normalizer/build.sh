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

# The cd and md modules are compiled with Cython. setup.py drops back to the
# pure-python modules silently when Cython is missing; the backend refuses
# instead, so a missing python3-cython fails here rather than shipping slower.
CHARSET_NORMALIZER_USE_CYTHON=1 pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
