# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# The move lookup tables are generated at configure time by a Perl script that
# needs Path::Tiny. It is a build tool only, taken from its CPAN tarball
# unpacked beside the source and never installed; without it the configure
# stops on the generator's failure.
export PERL5LIB="$SRC_ROOT/Path-Tiny-$_pathtiny/lib"

# KPatience links the shared library for its Golf hints. The test suite needs
# Perl modules this tree does not carry. An install RPATH would point at
# /usr/lib, where the loader looks anyway.
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
