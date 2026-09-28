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

# ONE PACKAGE, EVERY x86-64. GGML_NATIVE would tune to the build machine and
# SIGILL elsewhere. GGML_CPU_ALL_VARIANTS builds a CPU backend per
# microarchitecture (sse42 through zen4 and sapphirerapids) as modules under
# GGML_BACKEND_DIR, and libggml loads the best one the running CPU supports;
# it requires GGML_BACKEND_DL, which requires BUILD_SHARED_LIBS. A module
# missing from /usr/lib/ggml is a program that finds no CPU backend.
#
# OpenMP is libgomp from the gcc port and drives the CPU backend's threads.
# BLAS is OpenBLAS, a module the scheduler hands large matrix products to.
# Both are probes that disable themselves when the library is missing, so each
# is checked below rather than trusted. GGML_LLAMAFILE is the tinyBLAS matrix
# kernels, on by default only when ggml is built inside llama.cpp; a shared
# ggml without it makes every consumer slower than its own bundled copy.
#
# Vulkan is the GPU backend, one more module: mesa's radv, anv and nvk answer
# it, and a machine whose only Vulkan driver is lavapipe lists no device and
# runs on the CPU modules. Its shaders are compiled at build time by glslc.
# No CUDA, HIP, SYCL or OpenCL: none has a runtime here. No RPC: it is a
# network server that hands the machine's backends to other hosts.
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
