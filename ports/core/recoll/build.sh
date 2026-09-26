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

# CLI ONLY — --disable-qtgui, and there is no Qt on this host by rule. What
# remains is `recollindex` and `recollq`, which is the whole of what an offline
# machine needs: one index over the corpus, queried from a prompt or from any
# program that can read a list of paths.
#
# IT RIDES THE XAPIAN THE KIWIX STACK ALREADY NEEDS, which is what makes this
# cheap rather than a second search engine — the same library that gives a ZIM
# full-text search gives the local filesystem one.
#
# THE FILTERS SHELL OUT AND POPPLER IS WHY IT IS A DEPENDENCY. recoll indexes a
# PDF by running pdftotext; with no poppler it walks a directory of PDFs,
# reports success, and produces an index containing none of them. That is the
# failure this recipe's depends line exists to prevent, and it is silent.
# MESON, AND THE BUILD ROOT IS src/. 1.44 dropped autotools; the top-level
# CMakeLists is two lines that descend into src/, and src/ is where both the
# meson and the cmake definitions live.
#
# recollq is OFF BY DEFAULT and is half of what this port exists for, so it is
# named explicitly rather than assumed. rclgrep, also off by default, runs the
# same filters over a tree with no index at all. ext4-birthtime reads creation
# times through statx, which the kernel and musl both have. qtgui, webkit and
# webpreview are the hard rule; python-chm wants libchm, which is not a port;
# x11mon needs Xlib, and there is no Xorg here.
#
# ASPELL IS TWO OPTIONS AND NEEDS BOTH. `aspell` has recollindex build a
# spelling dictionary from the index with the `aspell` command; `python-aspell`
# builds the recollaspell module that rclaspell-sugg.py loads, and on Linux
# that script is the only route a suggestion takes — with the first and not
# the second, a query that matches nothing suggests nothing.
cd src
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dqtgui=false -Dwebkit=false -Dwebpreview=false \
	-Dpython-chm=false -Dpython-aspell=true -Daspell=true \
	-Dx11mon=false -Dsystemd=false -Dlibmagic=true \
	-Drecollq=true -Dindexer=true -Drclgrep=true -Dext4-birthtime=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
