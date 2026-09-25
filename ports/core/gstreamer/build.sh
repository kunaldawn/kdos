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

# libunwind (libunwind-nongnu) is what gst_debug_get_stack_trace() and the
# leaks tracer's stack-traces-flags walk the stack with; musl has no
# backtrace(), so without it both print nothing. libdw adds the source file and
# line to each frame. Both are named, so a missing one fails setup.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Dintrospection=disabled \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dptp-helper=disabled \
	-Dlibdw=enabled \
	-Dlibunwind=enabled \
	-Dbash-completion=enabled \
	-Dnls=disabled \
	-Dgst_debug=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
