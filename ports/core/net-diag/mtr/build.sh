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

# jansson, ncursesw and libcap are each found by a configure test that falls
# back to off without an error, so the three are named in depends: a missing
# one is a build that quietly loses JSON output, the TUI or its privilege drop.
./configure \
	--prefix=/usr \
	--mandir=/usr/share/man \
	--without-gtk \
	--with-jansson \
	--with-ncursesw \
	--with-ipinfo \
	--with-bashcompletiondir=/usr/share/bash-completion/completions
make
make DESTDIR=$PKG install
