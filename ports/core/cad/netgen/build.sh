# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/version-git.patch"
patch -p1 -i "$PORT_SRC/config-no-source-dir.patch"

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
