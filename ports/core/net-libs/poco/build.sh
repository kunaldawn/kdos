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

# POCO_UNBUNDLED links zlib, pcre2, utf8proc and expat from their ports; left
# off, each is compiled in from dependencies/ and no update to the port reaches
# it. FastLogger is off because it compiles a vendored copy of Quill.
# The libraries are the general-purpose set: Foundation, XML, JSON, Util, Net,
# Crypto, NetSSL, JWT, Zip and Encodings. Data and its SQL back ends, MongoDB,
# Redis, Prometheus, ActiveRecord, PageCompiler, PDF, SevenZip and DNS-SD are
# off; each is a closed feature nothing in the tree links.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DPOCO_UNBUNDLED=ON \
	-DENABLE_FASTLOGGER=OFF \
	-DENABLE_TRACE=OFF \
	-DENABLE_ENCODINGS=ON \
	-DENABLE_XML=ON \
	-DENABLE_JSON=ON \
	-DENABLE_UTIL=ON \
	-DENABLE_NET=ON \
	-DENABLE_CRYPTO=ON \
	-DENABLE_NETSSL=ON \
	-DENABLE_JWT=ON \
	-DENABLE_ZIP=ON \
	-DENABLE_DATA=OFF \
	-DENABLE_DATA_SQLITE=OFF \
	-DENABLE_DATA_MYSQL=OFF \
	-DENABLE_DATA_POSTGRESQL=OFF \
	-DENABLE_DATA_ODBC=OFF \
	-DENABLE_MONGODB=OFF \
	-DENABLE_REDIS=OFF \
	-DENABLE_PROMETHEUS=OFF \
	-DENABLE_ACTIVERECORD=OFF \
	-DENABLE_ACTIVERECORD_COMPILER=OFF \
	-DENABLE_PAGECOMPILER=OFF \
	-DENABLE_PAGECOMPILER_FILE2PAGE=OFF \
	-DENABLE_APACHECONNECTOR=OFF \
	-DENABLE_PDF=OFF \
	-DENABLE_SEVENZIP=OFF \
	-DENABLE_DNSSD=OFF \
	-DENABLE_CPPPARSER=OFF \
	-DENABLE_POCODOC=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_SAMPLES=OFF \
	-DENABLE_BENCHMARK=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
