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

patch -p1 -i "$PORT_SRC/gcc-15.patch"

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
