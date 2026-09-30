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

# The language model, the dictionary and the table set are three separate
# downloads upstream's data/CMakeLists.txt fetches with file(DOWNLOAD) — 49 MB
# that `make build --network none` cannot go and get. Its Fcitx5Download.cmake
# skips the fetch when the file is already there, and it resolves the name
# against the source directory, so putting them in data/ is the whole of it.
cp "$PORT_SRC/$_lm" "$PORT_SRC/$_dict" "$PORT_SRC/$_table" data/

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D ENABLE_TEST=Off \
	-D ENABLE_DOC=Off \
	-D ENABLE_DATA=On \
	-D ENABLE_TOOLS=On \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
