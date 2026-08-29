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

mv $SRC_ROOT/cmake-${version}.src $SRC_ROOT/cmake
mv $SRC_ROOT/runtimes-${version}.src $SRC_ROOT/runtimes

# ASM HAS TO BE ENABLED OR THE LIBRARY IS SILENTLY HALF-BUILT. libunwind's
# CMakeLists declares no project() — it is written to be a subdirectory of
# `runtimes`, whose own is `project(Runtimes C CXX ASM)`. Built directly, CMake
# supplies an implicit project() with C and CXX only, drops
# UnwindRegistersSave.S and UnwindRegistersRestore.S, and produces a
# libunwind.a that compiles, installs, and then fails EVERY static link with
# `undefined reference to __unw_getcontext` and
# `__libunwind_Registers_x86_64_jumpto`.
#
# The runtimes entry point is not the answer here: it includes AddLLVM,
# HandleLLVMOptions and GetHostTriple from ../llvm/cmake/modules, which is the
# 140 MB llvm source tarball, for three files. CMAKE_PROJECT_TOP_LEVEL_INCLUDES
# is included BY project() — the implicit one included — so one line of ours
# enables the language and nothing upstream is touched.
printf 'enable_language(ASM)\n' > "$SRC_ROOT/enable-asm.cmake"

cmake -B build -G Ninja \
	-D CMAKE_PROJECT_TOP_LEVEL_INCLUDES="$SRC_ROOT/enable-asm.cmake" \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-D LLVM_ENABLE_RUNTIMES="libunwind" \
	-D LIBUNWIND_INSTALL_HEADERS=ON \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build
