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

# A RUNTIME DEPENDENCY OF skyfield, AND A PORT RATHER THAN A VENDORED WHEEL.
# Every python recipe here installs --no-deps, so a vendored runtime set is
# never used; carrying these as ports is what makes `import skyfield` work on
# the target. Pure python, no build step beyond setuptools.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
