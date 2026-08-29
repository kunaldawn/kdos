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

# A BUILD BACKEND, AND THEREFORE A PORT RATHER THAN A VENDORED WHEEL. Under
# pip's build isolation every consumer would fetch this again from PyPI, and
# meson-python's own chain then reaches the PyPI `ninja` wrapper, whose sdist
# builds CMAKE FROM SOURCE — wrappers that exist for platforms with no system
# cmake, on a tree that has cmake, ninja and meson as ports already.
#
# --no-build-isolation for the same reason, one level down: the only thing an
# isolated environment would install here is setuptools, which is a port.
#
# THE 9.x LINE, AND ITS CONSUMER SAYS SO. matplotlib requires
# `setuptools_scm>=7,<10`, and 10 additionally split its VCS backend into a
# separate `vcs_versioning` distribution — so the newest is both out of range
# and a second port. 9.2.2 is the last of the line and needs neither.

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
