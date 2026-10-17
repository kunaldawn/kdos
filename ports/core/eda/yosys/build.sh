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

mkdir -p unpacked
tar xf "$PORT_SRC/$name-$version.tar.gz" -C unpacked
cd unpacked

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DYOSYS_USE_BUNDLED_LIBS=OFF \
	-DYOSYS_WITH_PYTHON=OFF \
	-DYOSYS_WITHOUT_ZLIB=OFF \
	-DYOSYS_WITHOUT_LIBFFI=OFF \
	-DYOSYS_WITHOUT_READLINE=OFF \
	-DYOSYS_WITHOUT_TCL=OFF \
	-DYOSYS_WITHOUT_EDITLINE=ON \
	-DYOSYS_ENABLE_UNIT_TESTS=OFF \
	-DYOSYS_ENABLE_FUNCTIONAL_TESTS=OFF \
	-DYOSYS_INSTALL_DRIVER=ON
for f in ZLIB LIBFFI READLINE TCL; do
	grep -q "^#define YOSYS_ENABLE_$f\$" kernel/yosys_config.h || {
		echo "yosys: YOSYS_ENABLE_$f is off — its library was not found" >&2
		exit 1
	}
done
ninja
DESTDIR=$PKG ninja install
