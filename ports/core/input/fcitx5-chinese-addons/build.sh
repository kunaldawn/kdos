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

tar -xf "$PORT_SRC/$name-$version.tar.zst" --strip-components=1

cp "$PORT_SRC/$_pytable" "$PORT_SRC/$_pystroke" modules/pinyinhelper/

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D ENABLE_GUI=Off \
	-D ENABLE_BROWSER=Off \
	-D ENABLE_CLOUDPINYIN=Off \
	-D ENABLE_OPENCC=On \
	-D ENABLE_TEST=Off \
	-D ENABLE_DATA=On \
	-D ENABLE_TOOLS=On \
	-D CMAKE_DISABLE_FIND_PACKAGE_Fcitx5ModuleLuaAddonLoader=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
