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
# The compiled components are the union of what the Boost-using catalogue
# names in a `find_package(... REQUIRED)`, and nothing else; everything else
# stays header-only, which is most of Boost. A missing one is a configure error
# naming `boost_<x>Config.cmake` rather than anything about boost:
#   - iostreams: libime, the input-method engine behind pinyin;
#   - date_time filesystem regex test: ledger;
#   - program_options thread: prjtrellis, nextpnr and FreeCAD;
#   - atomic charconv chrono container locale: GnuCash;
#   - random coroutine context graph: Wesnoth, which also takes charconv and
#     process when they are there;
#   - chrono: gr-osmosdr, and with it Gqrx;
#   - log (log and log_setup): WSJT-X, PrusaSlicer and OrcaSlicer;
#   - nowide: PrusaSlicer and OrcaSlicer;
#   - serialization: UHD;
#   - stacktrace (stacktrace_basic): Inkscape's crash report. The
#     stacktrace_backtrace variant is named off: it builds whenever a
#     libbacktrace happens to be found, and libbacktrace is not a port;
#   - python: libtorrent-rasterbar's Python binding, which Deluge imports, and
#     LinuxCNC, which links libboost_python3NN.
# system is a stub library Boost keeps for the consumers that still name it.
#
# THE LIST MUST BE ON BOTH LINES. bootstrap decides what b2 CAN build; b2's own
# --with- flags decide what it DOES. With the list on bootstrap alone, exactly
# one component is built and every consumer of the others fails at
# find_package with the library sitting uninstalled in the work tree.
#
# Changing this list rebuilds Boost and forces a libime rebuild
# with it, because libime's link line depends on what is built here.
#
# --with-icu=/usr GIVES Boost.Regex ITS UNICODE SIDE (u32regex) and Boost.Locale
# its ICU backend. Left to itself bootstrap turns ICU on when it happens to find
# the headers, so both link lines would follow build order; icu is in `depends`.
# --with-python names the interpreter Boost.Python is built for. Boost.Python
# also builds boost_numpy whenever `import numpy` works in the build root, so
# numpy is in `depends` for the same reason.
./bootstrap.sh --prefix=/usr --libdir=/usr/lib \
	--with-icu=/usr \
	--with-python=/usr/bin/python3 \
	--with-libraries=iostreams,system,filesystem,regex,date_time,test,program_options,thread,atomic,charconv,chrono,container,locale,random,coroutine,context,graph,process,log,nowide,serialization,stacktrace,python

# b2 reads no flags from the environment. The toolset declaration carries
# them, as Alpine's does: a flag given as a b2 argument would be split at its
# commas. b2 puts cflags (C) and cxxflags (C++) ahead of its own options, so
# the release variant's -O3 still decides the level; linkflags go last on the
# link line. bootstrap's project-config.jam declares gcc only when no
# configuration has, so this one is the one used.
cat > user-config.jam <<JAM
using gcc : : $CXX : <cflags>"$CFLAGS" <cxxflags>"$CXXFLAGS" <linkflags>"$LDFLAGS" ;
JAM
./b2 \
	--user-config=user-config.jam \
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
	--with-atomic \
	--with-charconv \
	--with-chrono \
	--with-container \
	--with-locale \
	--with-random \
	--with-coroutine \
	--with-context \
	--with-graph \
	--with-process \
	--with-log \
	--with-nowide \
	--with-serialization \
	--with-stacktrace \
	--with-python \
	boost.stacktrace.backtrace=off \
	variant=release \
	link=shared \
	threading=multi \
	install
