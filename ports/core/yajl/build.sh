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

# yajl 2.1.0's CMakeLists declares cmake_minimum_required(2.x);
# CMake >= 4.0 dropped that compat. Force a policy floor of 3.5.
#
# The reformatter/verify subdirs use GET_TARGET_PROPERTY(LOCATION),
# removed in CMake 4. They build the json_reformat / json_verify
# CLI tools — neither is required by library consumers (crun etc.).
# Drop them from the top-level CMakeLists and keep the library build.
sed -i \
	-e '/ADD_SUBDIRECTORY(reformatter)/d' \
	-e '/ADD_SUBDIRECTORY(verify)/d' \
	CMakeLists.txt

mkdir build
cd build
cmake .. \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5
make
make DESTDIR=$PKG install
