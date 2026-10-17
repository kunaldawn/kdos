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

cd bcc

patch -p1 -i $PORT_SRC/llvm23-mccontext.patch

mkdir -p build && cd build
cmake .. -G Ninja \
	-DREVISION=$version \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_USE_LIBBPF_PACKAGE=ON \
	-DENABLE_LLVM_SHARED=OFF \
	-DENABLE_CLANG_JIT=ON \
	-DENABLE_LIBDEBUGINFOD=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_LibDebuginfod=TRUE \
	-DCMAKE_DISABLE_FIND_PACKAGE_LuaJIT=TRUE \
	-DENABLE_MAN=ON \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_USDT=ON \
	-DPYTHON_CMD=python3
ninja
DESTDIR=$PKG ninja install

export EXTRA_LDFLAGS=-largp
_mk=(USE_BLAZESYM=0 BPFTOOL=/usr/sbin/bpftool prefix=/usr APP_PREFIX=bpf-
	CFLAGS="$CFLAGS -Wno-error=missing-field-initializers")
make -C ../libbpf-tools "${_mk[@]}"
make -C ../libbpf-tools "${_mk[@]}" DESTDIR=$PKG install
