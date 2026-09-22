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

# Source/GmmLib/CMakeLists.txt sets CMAKE_DISABLE_IN_SOURCE_BUILD, so the
# out-of-source -S/-B form is the only one that configures.
#
# The tree declares `cmake_minimum_required(VERSION 3.5)`, which cmake 4
# refuses outright; the policy floor is what lets it configure at all.
#
# RUN_TEST_SUITE defaults ON and is not a run-the-tests-later switch: ULT's
# `Run_ULT` is an ALL target whose POST_BUILD command executes the test binary,
# so leaving it on runs a Nehalem-baseline executable on the build machine
# during `cmake --build` and fails the build wherever that cannot run.
#
# Upstream's Linux.cmake feeds -march=${GMMLIB_MARCH} and the full SSE4.2 list
# through add_compile_options, which cmake emits after CMAKE_C_FLAGS and
# CMAKE_CXX_FLAGS, so the baseline for this library is corei7 whatever the
# environment asks for; -DGMMLIB_MARCH=<arch> is the only lever on it.
#
# CMAKE_INSTALL_LIBDIR is read twice: GNUInstallDirs feeds it into igdgmm.pc's
# libdir as well as choosing where the library lands, so a default of lib64
# would ship a .pc pointing at a directory this tree does not have.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DRUN_TEST_SUITE=OFF

cmake --build build
DESTDIR=$PKG cmake --install build
