# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

rmdir submodules/fast_float
cp -r "$SRC_ROOT/fast_float-$_ffver" submodules/fast_float

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
