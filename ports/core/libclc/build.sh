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

for triple in amdgcn-amd-amdhsa-llvm nvptx64-nvidia-cuda \
		spirv32-unknown-unknown spirv64-unknown-unknown; do
	cmake -S libclc -B build-$triple -G Ninja \
		-D CMAKE_INSTALL_PREFIX=/usr \
		-D CMAKE_BUILD_TYPE=Release \
		-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
		-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
		-D CMAKE_CLC_COMPILER=/usr/bin/clang \
		-D LLVM_DIR=/usr/lib/cmake/llvm \
		-D LLVM_DEFAULT_TARGET_TRIPLE=$triple \
		-D LIBCLC_USE_SPIRV_BACKEND=ON \
		-D LLVM_INCLUDE_TESTS=OFF

	cmake --build build-$triple
	DESTDIR=$PKG cmake --install build-$triple
done
