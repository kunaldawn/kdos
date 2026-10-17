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

_pyinc=$(python3 -c 'import sysconfig; print(sysconfig.get_path("include"))')
_pylib=$(python3 -c 'import sysconfig; print(sysconfig.get_config_var("LIBDIR") + "/" + sysconfig.get_config_var("LDLIBRARY"))')

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DPYTHON_EXECUTABLE=/usr/bin/python3 \
	-DPYTHON_INCLUDE_DIR="$_pyinc" \
	-DPYTHON_LIBRARY="$_pylib" \
	-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)" \
	-DLIBHIDAPI_INCLUDE_DIR=/usr/include/hidapi \
	-DLIBHIDAPI_LIBRARIES=/usr/lib/libhidapi-hidraw.so \
	-DENABLE_DOXYGEN=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/libgnuradio-funcube.so"
test -n "$(find "$PKG/usr/share/gnuradio/grc/blocks" -name 'funcube_*.block.yml' | head -1)"
