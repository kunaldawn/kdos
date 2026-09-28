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

# The 3.1.2 release declares its metadata in flit's [tool.flit.metadata]
# table, which flit_core 4 refuses; the patch is upstream's move to the
# standard [project] table, so the tree's flit_core builds it.
patch -p1 -i "$PORT_SRC/flit-project-table.patch"
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
