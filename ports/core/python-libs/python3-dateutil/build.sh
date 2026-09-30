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

# A RUNTIME DEPENDENCY OF pandas AND matplotlib — both import it at load,
# so without this port neither imports at all.
#
# setuptools_scm is in the backend and there is no .git in the sdist; it reads
# the version from PKG-INFO, which the sdist carries. --no-build-isolation
# reaches setuptools and setuptools_scm, both ports.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
