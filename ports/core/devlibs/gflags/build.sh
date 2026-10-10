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

# REGISTER_INSTALL_PREFIX=OFF: on, the install writes a package-registry entry
# into the builder's home directory, outside $PKG, and the package would not
# carry what the build wrote. The library directory is left at its default,
# lib: gflags declares it a PATH variable, so a value given on the command
# line is taken relative to the build directory and the library, its CMake
# package and gflags.pc install there instead of under /usr.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DBUILD_TESTING=OFF \
	-DBUILD_PACKAGING=OFF \
	-DREGISTER_BUILD_DIR=OFF \
	-DREGISTER_INSTALL_PREFIX=OFF
ninja
DESTDIR=$PKG ninja install
