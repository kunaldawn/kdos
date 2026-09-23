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
# -DBUILD_PYTHON=ON embeds Python for --pre-pack, --post-route and --run
# scripts, bound through pybind11. find_package(pybind11 CONFIG) finds nothing
# on the default search path, because the python3-pybind11 wheel keeps its
# CMake package inside site-packages, and a miss falls back to the copy in
# 3rdparty/ without a word. pybind11_DIR names the system copy.
#
# -DUSE_OPENMP=ON parallelises the analytic placer through gcc's libgomp.
#
# machxo2 covers MachXO2 and MachXO3 from the same prjtrellis database ecp5
# reads. nexus, mistral and himbaechel are left out of ARCH: their databases
# come from prjoxide, the mistral source tree and apicula, none of them ports.
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
	-DARCH="ice40;ecp5;machxo2;generic" \
	-DBUILD_GUI=OFF \
	-DBUILD_PYTHON=ON \
	-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)" \
	-DBUILD_RUST=OFF \
	-DBUILD_TESTS=OFF \
	-DUSE_OPENMP=ON \
	-DICESTORM_INSTALL_PREFIX=/usr \
	-DTRELLIS_INSTALL_PREFIX=/usr
ninja
DESTDIR=$PKG ninja install
