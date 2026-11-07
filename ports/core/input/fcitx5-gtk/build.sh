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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D ENABLE_GTK2_IM_MODULE=OFF \
	-D ENABLE_GTK3_IM_MODULE=ON \
	-D ENABLE_GTK4_IM_MODULE=ON \
	-D ENABLE_GIR=OFF \
	-D FCITX5_GTK_VERSION=$version \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

immod=/usr/lib/gtk-3.0/3.0.0
cache=$(LD_LIBRARY_PATH="$PKG/usr/lib" gtk-query-immodules-3.0 "$PKG$immod/immodules/im-fcitx5.so")
grep -q '"fcitx5"' <<<"$cache"
printf '%s\n' "${cache//"$PKG"/}" > "$PKG$immod/immodules.cache"
