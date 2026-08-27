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
# named explicitly rather than assumed. qtgui, webkit and webpreview are the
# hard rule; python-chm wants libchm and aspell wants aspell, neither a port;
# x11mon needs Xlib, and there is no Xorg here. libmagic is not a port either,
# so type identification falls back to execing `file`, which toybox provides.
cd src
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dqtgui=false -Dwebkit=false -Dwebpreview=false \
	-Dpython-chm=false -Dpython-aspell=false -Daspell=false \
	-Dx11mon=false -Dsystemd=false -Dlibmagic=false \
	-Drecollq=true -Dindexer=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
