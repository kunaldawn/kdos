# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

_triple=$(cc -dumpmachine)
cmake -S runtimes -B build -G Ninja \
	-D CMAKE_C_COMPILER=clang \
	-D CMAKE_CXX_COMPILER=clang++ \
	-D CMAKE_C_COMPILER_TARGET="$_triple" \
	-D CMAKE_CXX_COMPILER_TARGET="$_triple" \
	-D CMAKE_ASM_COMPILER_TARGET="$_triple" \
	-D LLVM_DEFAULT_TARGET_TRIPLE="$_triple" \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG" \
	-D LLVM_ENABLE_RUNTIMES="compiler-rt" \
	-D LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
	-D LLVM_INCLUDE_TESTS=OFF \
	-D COMPILER_RT_INSTALL_PATH="$_resdir" \
	-D COMPILER_RT_DEFAULT_TARGET_ONLY=ON \
	-D COMPILER_RT_INCLUDE_TESTS=OFF \
	-D COMPILER_RT_BUILD_BUILTINS=ON \
	-D COMPILER_RT_BUILD_CRT=ON \
	-D COMPILER_RT_BUILD_PROFILE=ON \
	-D COMPILER_RT_BUILD_SANITIZERS=ON \
	-D COMPILER_RT_SANITIZERS_TO_BUILD="ubsan_minimal" \
	-D COMPILER_RT_BUILD_XRAY=OFF \
	-D COMPILER_RT_BUILD_LIBFUZZER=OFF \
	-D COMPILER_RT_BUILD_MEMPROF=OFF \
	-D COMPILER_RT_BUILD_ORC=OFF \
	-D COMPILER_RT_BUILD_CTX_PROFILE=OFF \
	-D COMPILER_RT_BUILD_GWP_ASAN=OFF \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build
