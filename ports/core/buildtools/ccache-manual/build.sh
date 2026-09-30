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

# THE MANUAL PAGE IS BUILT BY HAND BECAUSE THE DOCUMENTATION SWITCH HAS NO
# MAN-ONLY FORM. ENABLE_DOCUMENTATION=ON puts the HTML manual, authors, news
# and licence into the default target and the install, beside ccache.1; OFF
# drops all four and the page with them. Upstream's own generator script is
# what the doc target runs for the page, with the same arguments, so the page
# is the one a documentation build would install. Its second argument is
# pandoc, which the manual-page path never calls. It is a port of its own
# because asciidoctor is 31_compilers' and ccache is 30_foundation's.
mkdir -p build
doc/scripts/generate-manpage asciidoctor "" doc/ccache-doc.css "$version" \
	doc/manual.adoc build/ccache.1
install -Dm644 build/ccache.1 -t "$PKG/usr/share/man/man1"
