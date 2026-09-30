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

# The fragment, molecule and crystal plugins install their data from sibling
# directories of the source tree, named exactly molecules, crystals and
# fragments; a missing one is cloned from GitHub at build time, which has no
# network and fails. The tag archives unpack beside $SRC with the version in
# the name, so each is moved to the name the build looks for.
for d in molecules crystals fragments; do
	mv "$SRC_ROOT/$d-$version" "$SRC_ROOT/$d"
done

# Spglib, libmsym and JKQTPlotter are not ports, so space groups, symmetry
# and the spectra plots are off. The package installer downloads plugin
# packages from GitHub and is the only thing libarchive serves; it is off.
# genXrdPattern is taken from the system, never downloaded, and the XRD plot
# needs the plotter anyway. The Python path is compiled into the input
# generators that run scripts, so it names the system interpreter.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DENABLE_TESTING=OFF \
	-DBUILD_DOCUMENTATION=OFF \
	-DENABLE_RPATH=OFF \
	-DUSE_QT=ON \
	-DQT_VERSION=6 \
	-DUSE_OPENGL=ON \
	-DUSE_HDF5=OFF \
	-DUSE_MMTF=OFF \
	-DUSE_PYTHON=OFF \
	-DUSE_SPGLIB=OFF \
	-DUSE_LIBMSYM=OFF \
	-DUSE_PLOTTER=OFF \
	-DUSE_LIBARCHIVE=OFF \
	-DUSE_SYSTEM_GENXRDPATTERN=ON \
	-DBUILD_GPL_PLUGINS=OFF \
	-DUSE_EXTERNAL_NLOHMANN=ON \
	-DUSE_EXTERNAL_PUGIXML=ON \
	-DUSE_EXTERNAL_TOMLPLUSPLUS=ON \
	-DPython3_EXECUTABLE=/usr/bin/python3 \
	-DOpenGL_GL_PREFERENCE=GLVND
cmake --build build
DESTDIR=$PKG cmake --install build
