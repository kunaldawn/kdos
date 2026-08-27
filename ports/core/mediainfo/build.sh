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

# Project/CMake/CLI, not Project/CMake: the CLI tarball carries only the CLI's
# own CMakeLists, where libmediainfo's carries one at the CMake root.
# MEDIAINFO_CLI_STATIC defaults ON and would compile a second copy of
# libmediainfo into the binary — the port beside this one is the shared library
# this is meant to link against.
cd Project/CMake/CLI
cmake . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DMEDIAINFO_CLI_STATIC=OFF \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
make
make DESTDIR=$PKG install
