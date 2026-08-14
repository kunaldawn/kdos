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

# ONE compiled library, not all of them. libime is the only consumer in this
# tree and it needs the headers plus Boost::iostreams; building the rest would
# be forty minutes and ~200 MB of shared objects nothing links. `--with-` on
# both bootstrap and b2, because bootstrap only decides what b2 CAN build.
#
# Boost's own layout puts headers under include/boost and the CMake config under
# lib/cmake — libime does find_package(Boost CONFIG), so that config file is the
# whole point of installing rather than pointing at a source tree.
./bootstrap.sh --prefix=/usr --libdir=/usr/lib --with-libraries=iostreams
./b2 \
	--prefix=$PKG/usr \
	--libdir=$PKG/usr/lib \
	--with-iostreams \
	variant=release \
	link=shared \
	threading=multi \
	install
