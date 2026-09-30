# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The tag archive leaves every submodule empty. fast_float is the one the
# build reads from submodules/ (its headers, at the commit tagged v6.0.0);
# libzip and zlib come from their ports and cpp-base64 from Libraries/.
rmdir submodules/fast_float
cp -r "$SRC_ROOT/fast_float-$_ffver" submodules/fast_float

# The bindings install under include/Bindings/<language>, upstream's layout:
# lib3mfConfig.cmake names that path literally for its lib3mf::Cpp target,
# and OpenSCAD's FindLib3MF finds lib3mf_implicit.hpp by the Bindings/Cpp
# suffix, so a different include directory breaks both. STRIP_BINARIES=OFF:
# upstream's default links with -s and leaves a library no debugger can read.
# The library directory is typed: the project declares it as a PATH cache
# entry, which turns an untyped relative value into a path under the source
# tree.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR:PATH=lib \
	-DCMAKE_INSTALL_INCLUDEDIR:PATH=include \
	-DLIB3MF_BUILD_SHARED=ON \
	-DLIB3MF_BUILD_WASM=OFF \
	-DLIB3MF_TESTS=OFF \
	-DUSE_INCLUDED_ZLIB=OFF \
	-DUSE_INCLUDED_LIBZIP=OFF \
	-DUSE_INCLUDED_CPPBASE64=ON \
	-DUSE_INCLUDED_FASTFLOAT=ON \
	-DUSE_PLATFORM_UUID=OFF \
	-DSTRIP_BINARIES=OFF \
	-DBUILD_FOR_CODECOVERAGE=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
