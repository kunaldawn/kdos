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

# --no-build-isolation: every build requirement is an installed port, and an
# isolated build would fetch them from PyPI. HDF5_DIR points the in-tree backend
# at the system library; without MPI it links the serial HDF5.
export HDF5_DIR=/usr
export HDF5_MPI=OFF
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
