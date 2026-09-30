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

# link_if.hpp uses std::uint32_t without <cstdint>, which gcc 15's libstdc++
# no longer includes transitively.
patch -p1 -i "$PORT_SRC/gcc-15.patch"

# Only host/ is built; fpga/ and mpm/ are the radio's own gateware and
# embedded daemon. UHD_VERSION is given, because upstream otherwise asks git
# for a describe string that a release tarball cannot answer.
#
# The C API, the Python module and the uhd_* utilities are built; examples,
# tests and the Doxygen manual are not. The
# simulator and the Python utility module need ruamel.yaml, which is not a
# port, and neither is DPDK.
#
# The 4.10 line: 4.11 moves the MPM devices (N3xx, E3xx, X4xx) onto gRPC,
# which is not a port, and building it without them would drop those radios.
#
# The FPGA and firmware images a USRP loads at start are not in the source:
# uhd_images_downloader fetches them into /usr/share/uhd/images on request,
# and nothing does so on its own. uhd-usrp.rules installs under
# /usr/lib/uhd/utils, unused; fs/etc/udev/rules.d/70-kdos-sdr.rules grants the
# Ettus and NI vendor ids to dialout.
cmake -S host -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DUHD_VERSION=$version \
	-DENABLE_STATIC_LIBS=OFF \
	-DENABLE_LIBUHD=ON \
	-DENABLE_C_API=ON \
	-DENABLE_PYTHON_API=ON \
	-DENABLE_UTILS=ON \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_PYMOD_UTILS=OFF \
	-DENABLE_SIM=OFF \
	-DENABLE_DPDK=OFF \
	-DENABLE_USB=ON \
	-DENABLE_MAN_PAGES=OFF \
	-DENABLE_MANUAL=OFF \
	-DENABLE_DOXYGEN=OFF \
	-DENABLE_RFNOC_DEV=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

# The manual-page component installs its uncompressed pages by bare name from
# docs/, where they are not, and the install stops; the compressed one runs
# gzip into the build. The pages are installed from docs/manpages instead,
# one for each command the build put in /usr/bin.
for c in "$PKG"/usr/bin/*; do
	p=host/docs/manpages/${c##*/}.1
	if [ -e "$p" ]; then
		install -Dm644 "$p" -t "$PKG"/usr/share/man/man1
	fi
done

test -e "$PKG"/usr/lib/pkgconfig/uhd.pc
test -x "$PKG"/usr/bin/uhd_find_devices
test -e "$PKG"/usr/share/man/man1/uhd_find_devices.1
test -n "$(find "$PKG"/usr/lib -path '*/site-packages/uhd/__init__.py')"
