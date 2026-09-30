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

# LLAMA_USE_SYSTEM_GGML links the libggml port, so llama.cpp and whisper.cpp
# load one set of CPU, OpenBLAS and Vulkan backend modules from /usr/lib/ggml;
# the copy of ggml inside this tarball is not built. How those modules are
# chosen is in the libggml recipe.
#
# THE SERVER'S WEB PAGE IS NOT EMBEDDED. It is a Svelte application built by
# npm (LLAMA_BUILD_UI) or downloaded from a Hugging Face bucket
# (LLAMA_USE_PREBUILT_UI); the build has no network, so both are off and
# llama-server serves the OpenAI-compatible API alone. A page placed in a
# directory is served with --path.
#
# LLAMA_OPENSSL is HTTPS for -hf and --model-url, which download a model at
# start-up; off, a model is a file on the disk and a program never reaches the
# network. LLGUIDANCE is a Rust library fetched at build time. No tests and no
# examples: the tools under /usr/bin are the programs, and a test would ship.
# LLAMA_BUILD_IS_DEV defaults on and makes every tool report the release as
# a -dev snapshot.
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
