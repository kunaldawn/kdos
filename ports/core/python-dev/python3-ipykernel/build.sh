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

# The build hook writes the python3 kernel spec, which installs to
# /usr/share/jupyter/kernels/python3, one of jupyter_core's system data paths,
# so every Jupyter front end here finds the kernel. debugpy is the kernel's
# debugger backend: JupyterLab's debugger panel steps through a cell with it.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
test -e "$PKG"/usr/share/jupyter/kernels/python3/kernel.json
