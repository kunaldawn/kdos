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

# The database in the tarball is installed under /usr/share/lensfun and is the
# one every consumer reads. The helper scripts and their Python module are
# off: lensfun-update-data downloads a newer database at run time, and the
# module exists only to serve the scripts.
#
# INSTALL_PYTHON_MODULE is declared and never read; the module is built and
# installed whenever find_program finds python3. PYTHON=OFF is a value that
# find_program keeps and that the IF(PYTHON) around the module reads as false.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_STATIC=OFF \
	-DBUILD_TESTS=OFF \
	-DBUILD_LENSTOOL=OFF \
	-DBUILD_DOC=OFF \
	-DINSTALL_PYTHON_MODULE=OFF \
	-DINSTALL_HELPER_SCRIPTS=OFF \
	-DPYTHON=OFF
make
make DESTDIR=$PKG install
