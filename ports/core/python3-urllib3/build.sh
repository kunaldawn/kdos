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

# Pure python on hatchling. The version comes from the sdist's PKG-INFO
# through hatch-vcs; without hatch-vcs the backend refuses to load its hook.
# brotli, zstd, h2 and SOCKS are optional imports resolved at run time.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
