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

for d in molecules crystals fragments avogenerators; do
	mv "$SRC_ROOT/$d-$version" "$SRC_ROOT/$d"
done
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DENABLE_TESTING=OFF \
	-DBUILD_DOCUMENTATION=OFF \
	-DENABLE_RPATH=OFF \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
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
	-DBUILD_GPL_PLUGINS=OFF \
	-DUSE_EXTERNAL_NLOHMANN=ON \
	-DUSE_EXTERNAL_PUGIXML=ON \
	-DUSE_EXTERNAL_TOMLPLUSPLUS=ON \
	-DPython3_EXECUTABLE=/usr/bin/python3 \
	-DOpenGL_GL_PREFERENCE=GLVND
cmake --build build
DESTDIR=$PKG cmake --install build
test -e "$PKG/usr/lib/avogadro2/plugins/avogenerators/pyproject.toml"
