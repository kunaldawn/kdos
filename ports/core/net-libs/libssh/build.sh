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

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DWITH_GSSAPI=ON \
	-DWITH_ZLIB=ON \
	-DWITH_SFTP=ON \
	-DWITH_SERVER=ON \
	-DWITH_GCRYPT=OFF \
	-DWITH_MBEDTLS=OFF \
	-DWITH_NACL=OFF \
	-DWITH_FIDO2=ON \
	-DWITH_PKCS11_URI=OFF \
	-DWITH_PKCS11_PROVIDER=OFF \
	-DWITH_GEX=ON \
	-DWITH_PCAP=ON \
	-DWITH_EXEC=ON \
	-DWITH_SYMBOL_VERSIONING=ON \
	-DWITH_INSECURE_NONE=OFF \
	-DWITH_BLOWFISH_CIPHER=OFF \
	-DWITH_EXAMPLES=OFF \
	-DWITH_INTERNAL_DOC=OFF \
	-DUNIT_TESTING=OFF \
	-DCLIENT_TESTING=OFF \
	-DSERVER_TESTING=OFF \
	-DGSSAPI_TESTING=OFF \
	-DFUZZ_TESTING=OFF \
	-DWITH_BENCHMARKS=OFF
ninja
DESTDIR=$PKG ninja install
