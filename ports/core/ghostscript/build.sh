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

# THE BUNDLED TREES ARE DELETED, NOT DISABLED, and that is the only way this
# combination configures. Ghostscript decides local-vs-system per library by
# testing whether the directory EXISTS in the source tree, and it then refuses
# to mix: with `jpeg/` present and libtiff coming from pkg-config it stops at
# "Mixing local libtiff with shared libjpeg not supported". Removing the
# directories for the libraries this tree already ships is what Debian does,
# and it is also the point — the bundled copies build fine and then the machine
# carries two of each, and only one gets a security fix.
#
# ONLY WHAT THIS TREE ALREADY SHIPS *AND GS CAN BE POINTED AT* IS REMOVED.
# jbig2dec, freetype, lcms2mt, leptonica, tesseract and brotli STAY: gs's
# --with-jbig2dec takes a LOCAL SOURCE TREE and has no system-library path at
# all, so the jbig2dec port cannot be reached from here and removing the
# bundled copy is a configure ERROR rather than a lost feature; gs's lcms is a
# forked "mt" variant, its leptonica/tesseract pair is the OCR device and is
# version-locked to it, and neither brotli nor a matching freetype build is
# wired up here.
rm -rf jpeg libpng tiff zlib openjpeg

# NOT --enable-dynamic. It is upstream-deprecated, it is refused outright
# alongside the default hidden visibility, and with --with-drivers=ALL every
# driver is linked in — a dynamically loadable one would buy nothing.

./configure --prefix=/usr --libdir=/usr/lib \
	--with-system-libtiff --disable-gtk \
	--with-drivers=ALL --without-x
make so
make
make DESTDIR=$PKG install
make DESTDIR=$PKG soinstall
# The interpreter is also what `gs` means to every script written in the last
# forty years; upstream installs the versioned name only from soinstall.
install -d "$PKG/usr/share/ghostscript"
