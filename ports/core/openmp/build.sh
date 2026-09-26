# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# libomp is the runtime clang's driver links for -fopenmp, and the one it
# passes -fopenmp to the frontend for; gcc keeps its own libgomp, which clang
# cannot use. omp.h goes to /usr/include, where clang finds it and gcc does
# not look: gcc's own omp.h is in its private include directory, searched
# first.
#
# LIBOMP_INSTALL_ALIASES=OFF: the aliases are libgomp.so and libiomp5.so
# pointing at libomp, and libgomp.so is gcc's. Archer is off because it is an
# OMPT tool for ThreadSanitizer, and the compiler-rt port builds no TSan, which
# does not support musl.
# libomptarget is offloading to a GPU, which is a separate runtime.
cmake -S runtimes -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
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
