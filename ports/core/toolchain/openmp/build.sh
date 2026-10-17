# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

cmake -S runtimes -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG" \
	-D LLVM_ENABLE_RUNTIMES="openmp" \
	-D LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
	-D LLVM_INCLUDE_TESTS=OFF \
	-D LIBOMP_ENABLE_SHARED=ON \
	-D LIBOMP_INSTALL_ALIASES=OFF \
	-D LIBOMP_ARCHER_SUPPORT=OFF \
	-D OPENMP_ENABLE_LIBOMPTARGET=OFF \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build
