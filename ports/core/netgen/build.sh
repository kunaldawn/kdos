# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The tag archive has no .git, so the version is given in git-describe form
# (the tag's commit, zero commits past it); without it the headers and the
# Python metadata say 6.2.0. The version header is regenerated at build time
# by a separate cmake run that asks git again, and fails with no git at all;
# the patch hands that run the version configure already has.
patch -p1 -i "$PORT_SRC/version-git.patch"

# USE_SUPERBUILD=OFF: the superbuild downloads OpenCASCADE and zlib and
# expects the pybind11 submodule, which the tag archive leaves empty, so
# pybind11 comes from its port. USE_NATIVE_ARCH=OFF: its -march=native is
# PUBLIC on ngcore and would reach every consumer's compile line. The Tcl/Tk
# GUI is GLX-only through Togl and is not built; the netgen Python module and
# nglib are the interfaces FreeCAD uses. Headers go under include/netgen,
# not beside every other library's, since netgen installs top-level
# directories named core, general and include. Stub files need
# pybind11-stubgen, which is not a port.
_pybind11_dir=$(python3 -c 'import pybind11; print(pybind11.get_cmake_dir())')
cmake -S . -B build -G Ninja -Wno-dev \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DNETGEN_VERSION_GIT=v$version-0-g$_commit \
	-DUSE_SUPERBUILD=OFF \
	-DUSE_NATIVE_ARCH=OFF \
	-DUSE_GUI=OFF \
	-DUSE_PYTHON=ON \
	-DPREFER_SYSTEM_PYBIND11=ON \
	-Dpybind11_DIR="$_pybind11_dir" \
	-DPython3_EXECUTABLE=/usr/bin/python3 \
	-DUSE_OCC=ON \
	-DUSE_MPI=OFF \
	-DUSE_JPEG=OFF \
	-DUSE_MPEG=OFF \
	-DUSE_CGNS=OFF \
	-DUSE_NUMA=OFF \
	-DUSE_CCACHE=OFF \
	-DINSTALL_PROFILES=OFF \
	-DENABLE_UNIT_TESTS=OFF \
	-DBUILD_STUB_FILES=OFF \
	-DNG_INSTALL_DIR_BIN=bin \
	-DNG_INSTALL_DIR_LIB=lib \
	-DNG_INSTALL_DIR_CMAKE=lib/cmake/netgen \
	-DNG_INSTALL_DIR_INCLUDE=include/netgen \
	-DNG_INSTALL_DIR_RES=share
cmake --build build
DESTDIR=$PKG cmake --install build
