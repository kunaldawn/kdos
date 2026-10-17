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

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DLLAMA_BUILD_IS_DEV=OFF \
	-DLLAMA_USE_SYSTEM_GGML=ON \
	-DLLAMA_ALL_WARNINGS=OFF \
	-DLLAMA_BUILD_COMMON=ON \
	-DLLAMA_BUILD_TOOLS=ON \
	-DLLAMA_BUILD_SERVER=ON \
	-DLLAMA_BUILD_APP=ON \
	-DLLAMA_BUILD_TESTS=OFF \
	-DLLAMA_TESTS_INSTALL=OFF \
	-DLLAMA_BUILD_EXAMPLES=OFF \
	-DLLAMA_BUILD_UI=OFF \
	-DLLAMA_USE_PREBUILT_UI=OFF \
	-DLLAMA_OPENSSL=OFF \
	-DLLAMA_LLGUIDANCE=OFF \
	-DLLAMA_SUBPROCESS=ON \
	-DLLAMA_TOOLS_INSTALL=ON
cmake --build build
DESTDIR=$PKG cmake --install build
for bin in llama-cli llama-server llama-quantize; do
	[ -x "$PKG/usr/bin/$bin" ] || { echo "llama.cpp: $bin was not built" >&2; exit 1; }
done
[ ! -e "$PKG/usr/lib/libggml.so" ] ||
	{ echo "llama.cpp: installed its own libggml over the libggml port" >&2; exit 1; }
