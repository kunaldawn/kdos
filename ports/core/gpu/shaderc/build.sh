# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# glslc is the shader compiler Vulkan projects expect by name: ggml's Vulkan
# backend in whisper.cpp compiles its shaders with it at build time. shaderc
# is glslang plus SPIRV-Tools behind a friendlier interface, and each release
# names the pair it was cut against; this version's are the glslang and
# spirv-tools ports' versions, so the pin moves with theirs.
#
# The patch adds SHADERC_SYSTEM_DEPS, which builds against those ports' CMake
# packages instead of third_party/, which the release archive carries empty.
#
# Only the shared library ships. The static libshaderc and libshaderc_combined
# are meant to carry glslang and SPIRV-Tools inside them; built against the
# installed archives they carry neither, and their pkg-config files would link
# nothing that works.
patch -p1 -i "$PORT_SRC/shaderc-system-deps.patch"

cmake -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
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
