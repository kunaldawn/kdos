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

# fix-lfs64, no-update and no-werror are Alpine's: musl has no fopen64 or
# lseek64, the updater is a separate program that downloads release builds,
# and the pattern language builds with -Werror. no-network drops the startup
# task that checks for updates and posts telemetry, and the crash-log upload
# after a crash, so the program never contacts its server on its own.
patch -p1 -i "$PORT_SRC/fix-lfs64.patch"
patch -p1 -i "$PORT_SRC/no-update.patch"
patch -p1 -i "$PORT_SRC/no-werror.patch"
patch -p1 -i "$PORT_SRC/no-network.patch"

# The offline build installs the pattern library from a sibling directory
# named ImHex-Patterns; without it the package carries no patterns, magic or
# encodings and the build does not say so.
mv "$SRC_ROOT/ImHex-Patterns-ImHex-v$version" "$SRC_ROOT/ImHex-Patterns"

# The file dialogs go through the FileChooser portal (NFD_PORTAL), not GTK.
# .NET scripting is kept out of the search: this tree carries no .NET. X11 is
# kept out too: the window comes from GLFW, and ImHex itself calls no Xlib.
# Every compression library of the decompress plugin is required, so a
# missing one fails here rather than dropping a format.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
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

# The panel reads no SVG: the menu icon is a PNG rasterised from upstream's.
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 resources/icon.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/imhex.png"
