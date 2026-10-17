# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/shaderc-system-deps.patch"

cmake -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG" \
	-D SHADERC_SYSTEM_DEPS=ON \
	-D SHADERC_SKIP_TESTS=ON \
	-D SHADERC_SKIP_EXAMPLES=ON \
	-D SHADERC_SKIP_COPYRIGHT_CHECK=ON \
	-D SHADERC_ENABLE_WERROR_COMPILE=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

rm -f "$PKG/usr/lib/libshaderc.a" "$PKG/usr/lib/libshaderc_combined.a" \
	"$PKG/usr/lib/pkgconfig/shaderc_static.pc" \
	"$PKG/usr/lib/pkgconfig/shaderc_combined.pc"
