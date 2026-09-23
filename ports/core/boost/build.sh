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

# Boost's own layout puts headers under include/boost and the CMake config under
# lib/cmake — libime does find_package(Boost CONFIG), so that config file is the
# whole point of installing rather than pointing at a source tree.
#
# Eight compiled components, and the list is deliberate.
#
# `iostreams` is what libime — the input-method engine behind pinyin — links.
# The other seven are what the rest of the Boost-using catalogue asks for, and
# each one is named by a `find_package(... REQUIRED)` somewhere: ledger wants
# `date_time filesystem iostreams regex unit_test_framework`, prjtrellis wants
# `filesystem program_options system thread`, nextpnr wants
# `program_options iostreams thread`. A missing one is a configure error naming
# `boost_<x>Config.cmake` rather than anything about boost. Everything else
# stays header-only, which is most of Boost.
#
# THE LIST MUST BE ON BOTH LINES. bootstrap decides what b2 CAN build; b2's own
# --with- flags decide what it DOES. With the list on bootstrap alone, exactly
# one component is built and every consumer of the other seven fails at
# find_package with the library sitting uninstalled in the work tree.
#
# Changing this list rebuilds Boost and forces a libime rebuild
# with it, because libime's link line depends on what is built here.
#
# --with-icu=/usr GIVES Boost.Regex ITS UNICODE SIDE (u32regex). Left to
# itself bootstrap turns ICU on when it happens to find the headers, so the
# regex library's link line would follow build order; icu is in `depends`.
# Boost.Python is not built: ledger and nextpnr, the two consumers that could
# use it, both build with their Python bindings off.
./bootstrap.sh --prefix=/usr --libdir=/usr/lib \
	--with-icu=/usr \
	--with-libraries=iostreams,system,filesystem,regex,date_time,test,program_options,thread
./b2 \
	--prefix=$PKG/usr \
	--libdir=$PKG/usr/lib \
	--with-iostreams \
	--with-system \
	--with-filesystem \
	--with-regex \
	--with-date_time \
	--with-test \
	--with-program_options \
	--with-thread \
	variant=release \
	link=shared \
	threading=multi \
	install
