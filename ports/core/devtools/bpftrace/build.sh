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

patch -p1 -i $PORT_SRC/llvm-definitions.patch

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DUSE_SYSTEM_LIBBPF=ON \
	-DBUILD_TESTING=OFF \
	-DENABLE_MAN=ON \
	-DASCIIDOCTOR=/usr/bin/asciidoctor \
	-DENABLE_SKB_OUTPUT=ON \
	-DENABLE_SYSTEMD=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibBfd=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibOpcodes=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibDw=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibPcap=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LibBlazesym=ON \
	-DSTATIC_LINKING=OFF \
	-DCMAKE_CXX_STANDARD_LIBRARIES="$(llvm-config --libs)"
ninja
DESTDIR=$PKG ninja install
