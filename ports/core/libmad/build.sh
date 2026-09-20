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

# THE MAINTAINED FORK, NOT UPSTREAM'S 0.15.1b. That release is from 2004 and
# its configure injects -fforce-mem, a flag gcc removed; every distro carrying
# it carries a patch to strip that out. This fork replaced the build system
# with cmake and needs no source edits at all, which is the whole reason it is
# the version named here.
# CMAKE_POLICY_VERSION_MINIMUM IS THE FLAG CMAKE ITSELF NAMES. The fork still
# declares cmake_minimum_required(VERSION 3.0), and CMake 4 removed
# compatibility below 3.5 — it refuses to configure and says to pass this. The
# alternative is editing that one line in a shipped CMakeLists, which is a
# patch carried for ever against a project that will raise the floor itself.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D BUILD_SHARED_LIBS=True

cmake --build build
DESTDIR=$PKG cmake --install build
