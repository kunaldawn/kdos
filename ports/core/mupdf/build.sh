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

# USE_SYSTEM_LIBS: mupdf vendors seventeen libraries in thirdparty/. Building
# them is a second copy of each on the machine and only one of the two gets a
# security fix.
#
# THE FOUR EXCEPTIONS ARE NAMED, because USE_SYSTEM_LIBS=yes turns EVERY one of
# them on at once and a missing header is a build that dies a few files in.
# gumbo-parser and mujs are mupdf's own maintained forks with no upstream to be
# a port of; zxing-cpp has no port here; and lcms2mt is a forked variant of
# lcms2 that mupdf itself says is strongly preferred. The rest — freetype,
# harfbuzz, jbig2dec, libjpeg, openjpeg, zlib, curl, brotli, leptonica and
# tesseract — are ports and come from the system.
#
# The X11 and GLUT viewers are out — no Xorg server, and mupdf-gl is GLUT over
# X11 rather than Wayland. What ships is mutool and the shared library, which
# is the half a scriptable machine wants.
MUPDF_SYS="USE_SYSTEM_LIBS=yes USE_SYSTEM_GUMBO=no USE_SYSTEM_MUJS=no \
	USE_SYSTEM_ZXINGCPP=no USE_SYSTEM_LCMS2=no"

make $MUPDF_SYS HAVE_X11=no HAVE_GLUT=no HAVE_OBJCOPY=yes \
	build=release prefix=/usr shared=yes
make $MUPDF_SYS HAVE_X11=no HAVE_GLUT=no HAVE_OBJCOPY=yes \
	build=release prefix=/usr shared=yes DESTDIR=$PKG install
