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

# THE CHIP DATABASE IS A GIT SUBMODULE and a tag archive carries its directory
# EMPTY, so `database/` installs as nothing at all. Nothing here notices —
# prjtrellis builds and installs cleanly — and nextpnr then dies generating its
# ECP5 chipdb on `/usr/share/trellis/database/devices.json: cannot open file`.
# The revision is the one the 1.4 tag records for the submodule.
#
# BEFORE `cd libtrellis`: the install rule is `install(DIRECTORY ../database)`
# relative to libtrellis/, so the tree belongs at the TOP of $SRC. Unpacked
# one level down it is installed empty and nothing complains.
rm -rf database
mkdir -p database
tar xf "$PORT_SRC/prjtrellis-db-$_db.tar.gz" --strip-components=1 -C database

cd libtrellis

# ICE40 IS SMALL AND ECP5 IS THE ONE WITH ROOM. icestorm covers the iCE40
# family — a few thousand LUTs, enough for glue logic and a small soft core —
# and this covers the ECP5, which is where a RISC-V SoC with DDR3 and a PCIe
# lane actually fits. They are the two families the open flow supports end to
# end, and nextpnr needs the database of whichever one is being targeted
# INSTALLED BEFORE IT IS COMPILED.
#
# The database is ~200 MB of chip data and that is the port. There is nothing
# to trim: a partial fabric description is a placer that fails on a design
# using the corner of the chip somebody left out.
mkdir -p build && cd build
# CMP0167=NEW makes find_package(Boost) use BOOSTCONFIG.CMAKE rather than
# CMake's own legacy FindBoost module. The module looks for a `libboost_system`
# FILE on disk, and Boost 1.92 made Boost.System HEADER-ONLY — so it reports
# "Could NOT find Boost (missing: system)" beside a boost that is fully
# installed, right after warning that a new Boost "may have incorrect or
# missing dependencies". The config package upstream ships knows which of its
# own components are header-only.
#
# CURRENT_GIT_VERSION IS SET BECAUSE THERE IS NO .git HERE. libtrellis derives
# version.cpp from `git describe`, and in a tarball that command fails, leaves
# the variable empty and skips the configure_file — so every tool target ends
# up with "Cannot find source file: generated/version.cpp" and then "No SOURCES
# given". The CMakeLists guards the git call with `if (NOT DEFINED …)` for
# exactly this case.
#
# BUILD_PYTHON=ON IS NOT A PYTHON FEATURE HERE — it is what produces
# `pytrellis`, and nextpnr's FindTrellis.cmake REQUIRES that module to
# configure its ecp5 architecture at all. Without it nextpnr stops at "Failed
# to locate the pytrellis library" and the ECP5 half of the open FPGA flow does
# not exist. It costs no new dependency: libtrellis binds through the pybind11
# vendored beside it, not through boost::python.
#
# AND CMAKE_INSTALL_LIBDIR=lib, or GNUInstallDirs picks lib64 on x86_64 and
# pytrellis.so lands in /usr/lib64/trellis. nextpnr's FindTrellis.cmake
# searches CMAKE_SYSTEM_LIBRARY_PATH — /usr/lib and friends, never lib64 — so
# the module is built, installed, and reported missing.
cmake .. -G Ninja \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCURRENT_GIT_VERSION=$version \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED=ON \
	-DBUILD_PYTHON=ON
ninja
DESTDIR=$PKG ninja install
