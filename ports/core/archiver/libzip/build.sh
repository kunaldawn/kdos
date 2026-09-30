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

# Each compression method and the OpenSSL backend is an unrequired
# find_package that only warns when the library is missing. The
# CMAKE_REQUIRE_FIND_PACKAGE_<name> switches make each one fatal instead.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_DOC=ON \
	-DDOCUMENTATION_FORMAT=man \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_REGRESS=OFF \
	-DBUILD_TOOLS=ON \
	-DENABLE_COMMONCRYPTO=OFF \
	-DENABLE_GNUTLS=OFF \
	-DENABLE_MBEDTLS=OFF \
	-DENABLE_OPENSSL=ON \
	-DENABLE_BZIP2=ON \
	-DENABLE_LZMA=ON \
	-DENABLE_ZSTD=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_OpenSSL=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_BZip2=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibLZMA=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_zstd=ON
make
make DESTDIR=$PKG install
