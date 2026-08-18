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
# Five compiled components, and the list is deliberate.
#
# `iostreams` is what libime — the input-method engine behind pinyin — links.
# `system`, `filesystem`, `regex` and `date_time` are what the rest of the
# Boost-using catalogue asks for. Everything else stays header-only, which is
# most of Boost.
#
# Changing this list rebuilds Boost (~40 minutes) and forces a libime rebuild
# with it, because libime's link line depends on what is built here.
./bootstrap.sh --prefix=/usr --libdir=/usr/lib \
	--with-libraries=iostreams,system,filesystem,regex,date_time
./b2 \
	--prefix=$PKG/usr \
	--libdir=$PKG/usr/lib \
	--with-iostreams \
	variant=release \
	link=shared \
	threading=multi \
	install
