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

# Pinned to a COMMIT because upstream publishes no tags at all; the version is
# that commit's date, which is the only monotonic thing on offer.
#
# What this buys is worth being exact about, because "locales on musl" invites
# the wrong expectation: musl implements LC_MESSAGES and nothing else — no
# collation, no localised strftime, no LC_NUMERIC. So this installs the `locale`
# command and the message catalogues, and LANG=zh_CN.UTF-8 starts giving
# translated program output. It does not make musl a glibc.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D LOCALE_PROFILE=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
