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

# GNU Radio blocks that estimate and correct I/Q imbalance, which gr-osmosdr
# applies to a radio's stream. The release archive carries the libosmo-dsp
# submodule empty, so the libosmo-dsp port is linked instead; were it not
# found, the build would fall back to compiling the missing sources. The
# Doxygen reference is not built.
#
# CMP0167=NEW: find_package(Boost) reads Boost's own config package, which
# knows Boost.System is header-only. FindPythonLibs is handed the library and
# headers, because its version table is older than this python.
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
	-DENABLE_DOXYGEN=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/libgnuradio-iqbalance.so"
readelf -d "$PKG/usr/lib/libgnuradio-iqbalance.so" | grep -q libosmodsp
