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

# LIBBPF_EMBEDDED=OFF links the libbpf port instead of the copy upstream
# carries under lib/bpf. LIB_INSTALL_DIR is the variable upstream's install()
# reads for its libraries; left unset, they land under the prefix itself.
# Python3 is kept out so the package does not change with whether python3
# happens to be installed: its only use is ostra-cg, a ctracer helper.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DLIB_INSTALL_DIR=lib \
	-DLIBBPF_EMBEDDED=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Python3=ON
ninja
DESTDIR=$PKG ninja install
