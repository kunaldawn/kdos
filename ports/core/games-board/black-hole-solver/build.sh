# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export PERL5LIB="$SRC_ROOT/Path-Tiny-$_pathtiny/lib"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D FCS_WITH_TEST_SUITE=OFF \
	-D BUILD_STATIC_LIBRARY=OFF \
	-D FCS_AVOID_TCMALLOC=ON \
	-D DISABLE_APPLYING_RPATH=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
