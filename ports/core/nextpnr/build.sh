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

# THE CHIP DATABASES ARE READ AT BUILD TIME, NOT AT RUN TIME, which is why
# icestorm and prjtrellis are `depends` of a program that does not link them.
# nextpnr compiles each fabric description into its own binary — an ice40
# nextpnr and an ecp5 nextpnr are two executables — so a database installed
# after this port is a database this build never saw.
#
# -DBUILD_GUI=OFF is the hard rule: nextpnr's viewer is Qt, and the 3rdparty
# QtPropertyBrowser tree in the tarball exists only for it.
#
# -DBUILD_PYTHON=OFF drops the boost::python bindings; boost is still needed
# for filesystem and program_options, which the CLI itself uses.
mkdir -p build && cd build
# CMP0167=NEW makes find_package(Boost) use BOOSTCONFIG.CMAKE rather than
# CMake's own legacy FindBoost module. The module looks for a `libboost_system`
# FILE on disk, and Boost 1.92 made Boost.System HEADER-ONLY — so it reports
# "Could NOT find Boost (missing: system)" beside a boost that is fully
# installed, right after warning that a new Boost "may have incorrect or
# missing dependencies". The config package upstream ships knows which of its
# own components are header-only.
cmake .. -G Ninja \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DARCH="ice40;ecp5;generic" \
	-DBUILD_GUI=OFF \
	-DBUILD_PYTHON=OFF \
	-DBUILD_TESTS=OFF \
	-DUSE_OPENMP=OFF \
	-DICESTORM_INSTALL_PREFIX=/usr \
	-DTRELLIS_INSTALL_PREFIX=/usr
ninja
DESTDIR=$PKG ninja install
