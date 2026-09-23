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

# --disable-backtrace: musl has no execinfo, and btrfs-progs' own configure
# does not probe for it — leaving this on is an undefined-reference link error
# at the very end of a long build.
# --with-convert=ext2 names the one filesystem btrfs-convert reads; left at
# auto, configure also probes for reiserfscore, which is not a port.
# The manual pages are rendered with sphinx-build, the only thing the
# documentation target builds. conf.py loads sphinx_rtd_theme, an HTML theme
# that is not a port and that the man builder never uses; O= reaches
# Documentation/Makefile's sphinx-build command line, and -D extensions
# replaces conf.py's list with the man builder itself, already loaded.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--bindir=/usr/bin \
	--disable-static \
	--disable-backtrace \
	--disable-python \
	--enable-documentation \
	--enable-convert \
	--with-convert=ext2 \
	--enable-zoned \
	--enable-zstd \
	--enable-lzo \
	--enable-libudev \
	--with-crypto=builtin

_sphinx="-D extensions=sphinx.builders.manpage"
make O="$_sphinx"
make DESTDIR=$PKG O="$_sphinx" install
