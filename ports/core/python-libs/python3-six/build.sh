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

# A RUNTIME DEPENDENCY OF python-dateutil. Every python recipe here installs
# --no-deps, so a module another one imports is a port or it is absent on the
# target. --no-build-isolation: setuptools is the whole backend, and a port.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
