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

# GNU Radio's GrComponent is used here, so ENABLE_DEFAULT=OFF plus every
# wanted device forced ON makes a missing radio library a configure error
# rather than a missing entry in the device string parser. UHD goes through
# GNU Radio's gr-uhd, the FUNcube Dongle through gr-funcube, and IQ balance
# correction through gr-iqbal. Off: FreeSRP and XTRX, neither of which is a
# port; SDRplay is nonfree and never looked up.
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
	-DENABLE_DEFAULT=OFF \
	-DENABLE_NONFREE=OFF \
	-DENABLE_DOXYGEN=OFF \
	-DENABLE_PYTHON=ON \
	-DENABLE_FILE=ON \
	-DENABLE_RTL=ON \
	-DENABLE_RTL_TCP=ON \
	-DENABLE_HACKRF=ON \
	-DENABLE_BLADERF=ON \
	-DENABLE_AIRSPY=ON \
	-DENABLE_SOAPY=ON \
	-DENABLE_RFSPACE=ON \
	-DENABLE_REDPITAYA=ON \
	-DENABLE_UHD=ON \
	-DENABLE_FCD=ON \
	-DENABLE_AIRSPYHF=ON \
	-DENABLE_MIRI=ON \
	-DENABLE_FREESRP=OFF \
	-DENABLE_XTRX=OFF \
	-DENABLE_IQBALANCE=ON \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/libgnuradio-osmosdr.so"
test -x "$PKG/usr/bin/osmocom_fft"
