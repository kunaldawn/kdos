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

# The font and its icon list come from the Fork Awesome release beside this
# tarball; left unset, the build downloads both from GitHub.
_fa="$SRC_ROOT/Fork-Awesome-$_forkawesome"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D BUILD_TESTING=OFF \
	-D QT_PACKAGE_PREFIX=Qt6 \
	-D FORK_AWESOME_VERSION="$_forkawesome" \
	-D FORK_AWESOME_FONT_FILE="$_fa/fonts/forkawesome-webfont.ttf" \
	-D FORK_AWESOME_ICON_DEFINITIONS="$_fa/src/icons/icons.yml" \
	-D ENABLE_QT_QUICK_LIBRARY=ON \
	-D NO_DOXYGEN=ON \
	-D NO_SPHINX=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
