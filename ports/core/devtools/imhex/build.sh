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

patch -p1 -i "$PORT_SRC/fix-lfs64.patch"
patch -p1 -i "$PORT_SRC/no-update.patch"
patch -p1 -i "$PORT_SRC/no-werror.patch"
patch -p1 -i "$PORT_SRC/no-network.patch"

mv "$SRC_ROOT/ImHex-Patterns-ImHex-v$version" "$SRC_ROOT/ImHex-Patterns"

export CXXFLAGS="$CXXFLAGS -DFMT_DEPRECATED_HEAVY_CORE"
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_RPATH='$ORIGIN' \
	-DIMHEX_OFFLINE_BUILD=ON \
	-DIMHEX_IGNORE_BAD_CLONE=ON \
	-DIMHEX_USE_GTK_FILE_PICKER=OFF \
	-DIMHEX_STRICT_WARNINGS=OFF \
	-DIMHEX_STRIP_RELEASE=OFF \
	-DIMHEX_COMPRESS_DEBUG_INFO=OFF \
	-DIMHEX_DISABLE_STACKTRACE=ON \
	-DIMHEX_BUNDLE_DOTNET=OFF \
	-DIMHEX_BUNDLE_PLUGIN_SDK=OFF \
	-DIMHEX_ENABLE_UNIT_TESTS=OFF \
	-DIMHEX_ENABLE_PLUGIN_TESTS=OFF \
	-DUSE_SYSTEM_CAPSTONE=ON \
	-DUSE_SYSTEM_NLOHMANN_JSON=ON \
	-DUSE_SYSTEM_FMT=ON \
	-DUSE_SYSTEM_YARA=ON \
	-DUSE_SYSTEM_LLVM=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_CoreClrEmbed=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_BZip2=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibLZMA=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ZSTD=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LZ4=ON
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/imhex/sdk"

install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 resources/icon.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/imhex.png"
