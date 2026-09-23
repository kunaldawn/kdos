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

cmake -S . -B build -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_LIBEXECDIR=lib \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-DREDIS_STORAGE_BACKEND=OFF \
	-DENABLE_TESTING=OFF \
	-DENABLE_DOCUMENTATION=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# THE MANUAL PAGE IS BUILT BY HAND BECAUSE THE DOCUMENTATION SWITCH HAS NO
# MAN-ONLY FORM. ENABLE_DOCUMENTATION=ON puts the HTML manual, authors, news
# and licence into the default target and the install, beside ccache.1; OFF
# drops all four and the page with them. Upstream's own generator script is
# what the doc target runs for the page, with the same arguments, so the page
# is the one a documentation build would install. Its second argument is
# pandoc, which the manual-page path never calls.
doc/scripts/generate-manpage asciidoctor "" doc/ccache-doc.css "$version" \
	doc/manual.adoc build/ccache.1
install -Dm644 build/ccache.1 -t "$PKG/usr/share/man/man1"

install -d $PKG/usr/lib/ccache
for c in gcc g++ cc c++; do
	ln -s /usr/bin/ccache $PKG/usr/lib/ccache/$c
done
