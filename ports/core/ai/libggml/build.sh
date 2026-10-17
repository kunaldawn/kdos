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
	-DGGML_BUILD_TESTS=OFF \
	-DGGML_BUILD_EXAMPLES=OFF \
	-DGGML_ALL_WARNINGS=OFF \
	-DGGML_NATIVE=OFF -DGGML_CCACHE=OFF -DGGML_LTO=OFF \
	-DGGML_BACKEND_DL=ON -DGGML_CPU_ALL_VARIANTS=ON \
	-DGGML_BACKEND_DIR=/usr/lib/ggml \
	-DGGML_LLAMAFILE=ON \
	-DGGML_OPENMP=ON \
	-DGGML_BLAS=ON -DGGML_BLAS_VENDOR=OpenBLAS \
	-DGGML_VULKAN=ON -DGGML_CUDA=OFF -DGGML_HIP=OFF -DGGML_MUSA=OFF \
	-DGGML_SYCL=OFF -DGGML_OPENCL=OFF -DGGML_WEBGPU=OFF -DGGML_RPC=OFF
grep -q '^GGML_OPENMP_ENABLED:INTERNAL=ON$' build/CMakeCache.txt ||
	{ echo "libggml: OpenMP not found at configure" >&2; exit 1; }
cmake --build build
DESTDIR=$PKG cmake --install build
for mod in libggml-blas.so libggml-cpu-x64.so libggml-cpu-haswell.so \
	libggml-vulkan.so; do
	[ -f "$PKG/usr/lib/ggml/$mod" ] ||
		{ echo "libggml: $mod was not built" >&2; exit 1; }
done
[ -f "$PKG/usr/lib/cmake/ggml/ggml-config.cmake" ] ||
	{ echo "libggml: no CMake package for find_package(ggml)" >&2; exit 1; }
