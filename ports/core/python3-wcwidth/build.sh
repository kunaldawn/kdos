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

# ONE COPY FOR EVERY TERMINAL PROGRAM THAT MEASURES TEXT: prompt_toolkit
# (ipython), urwid (khal, toot) and toot itself import it. A module two
# packages each installed would be a path both own.
#
# The C extension is optional upstream and a failed compile falls back to the
# pure-python tables without a word. CIBUILDWHEEL is the switch that makes the
# extension a hard requirement, so the package is the same on every build.
CIBUILDWHEEL=1 pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
