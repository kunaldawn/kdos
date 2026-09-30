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

# Qt-Color-Widgets is the colour picker, pinned by commit in CMakeLists.txt.
# Found under external/ it is added EXCLUDE_FROM_ALL and linked statically;
# anywhere else CMake clones it, and its install rules would put a static
# library and headers into the package.
mkdir -p external
mv "$SRC_ROOT/Qt-Color-Widgets-$_qcw" external/Qt-Color-Widgets

# Offline and nothing reaching out: the Imgur uploader and the update checker
# are compiled out. The clipboard goes through KGuiAddons' Wayland data-control
# path, or a copied capture dies with the process that owned it. The launcher
# entry names "flameshot", not /usr/bin/flameshot, so it resolves like every
# other entry. Git is not searched for: the tarball is no checkout, and the
# version string then reads the same on every builder.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_DISABLE_FIND_PACKAGE_Git=ON \
	-D USE_KDSINGLEAPPLICATION=ON \
	-D USE_BUNDLED_KDSINGLEAPPLICATION=OFF \
	-D USE_WAYLAND_CLIPBOARD=ON \
	-D USE_LAUNCHER_ABSOLUTE_PATH=OFF \
	-D ENABLE_IMGUR=OFF \
	-D DISABLE_UPDATE_CHECKER=ON \
	-D USE_MONOCHROME_ICON=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# Bundled data is English only, and Qt falls back to the source strings.
rm -rf "$PKG/usr/share/flameshot/translations"
