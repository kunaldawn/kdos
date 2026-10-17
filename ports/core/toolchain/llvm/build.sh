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

cmake -S llvm -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D LLVM_HOST_TRIPLE="$(cc -dumpmachine)" \
	-D LLVM_DEFAULT_TARGET_TRIPLE="$(cc -dumpmachine)" \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -include cstdint -DNDEBUG" \
	-D LLVM_BINUTILS_INCDIR=/usr/include \
	-D LLVM_BUILD_LLVM_DYLIB=OFF \
	-D LLVM_LINK_LLVM_DYLIB=OFF \
	-D BUILD_SHARED_LIBS=ON \
	-D LLVM_PARALLEL_COMPILE_JOBS="$(echo "$MAKEFLAGS" | grep -o '[0-9]*')" \
	-D LLVM_INCLUDE_EXAMPLES=OFF \
	-D LLVM_INCLUDE_TESTS=OFF \
	-D LLVM_ENABLE_FFI=ON \
	-D LLVM_ENABLE_RTTI=ON \
	-D LLVM_ENABLE_ZLIB=FORCE_ON \
	-D LLVM_ENABLE_ZSTD=FORCE_ON \
	-D LLVM_ENABLE_LIBXML2=FORCE_ON \
	-D LLVM_ENABLE_LIBEDIT=OFF \
	-D LLVM_ENABLE_LIBPFM=OFF \
	-D LLVM_ENABLE_Z3_SOLVER=OFF \
	-D LLVM_ENABLE_BINDINGS=OFF \
	-D LLVM_USE_PERF=ON \
	-D LLVM_ENABLE_OCAMLDOC=OFF \
	-D LLVM_INSTALL_UTILS=ON \
	-D LLVM_UTILS_INSTALL_DIR=lib/llvm/utils \
	-D LLVM_TOOL_LLVM_IR2VEC_BUILD=OFF \
	-D LLVM_ENABLE_LIBCXX=OFF \
	-D LLVM_ENABLE_LLD=OFF \
	-D LLVM_OPTIMIZED_TABLEGEN=ON \
	-D LLVM_INCLUDE_BENCHMARKS=OFF \
	-D LLVM_TARGETS_TO_BUILD=all \
	-D LLVM_ENABLE_SPHINX=ON \
	-D SPHINX_OUTPUT_HTML=OFF \
	-D SPHINX_OUTPUT_MAN=ON \
	-D SPHINX_WARNINGS_AS_ERRORS=OFF \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

cmake --build build --target docs-llvm-man
install -Dm644 build/docs/man/*.1 -t "$PKG/usr/share/man/man1"
rm -f "$PKG/usr/share/man/man1/llvm-ir2vec.1"
