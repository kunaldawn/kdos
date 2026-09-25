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
# a port of; zxing-cpp has no port here, so barcode support stays off; and
# lcms2mt is a forked variant of lcms2 that mupdf itself says is strongly
# preferred. The rest — freetype, harfbuzz, jbig2dec, libjpeg, openjpeg, zlib,
# curl, brotli, leptonica, tesseract and libarchive — are ports and come from
# the system.
#
# tesseract=yes is OCR output (`mutool draw -F ocr.pdf`), reading tesseract's
# eng.traineddata; archive=yes opens cbr, cb7 and tar comic archives through
# libarchive. Both stop the build with an error when their library is missing.
# HAVE_LIBCRYPTO=yes is PDF signing and signature checks: left unset it is a
# pkg-config probe that drops them without a word when openssl is absent.
#
# The X11 and GLUT viewers are out — no Xorg server, and mupdf-gl is GLUT over
# X11 rather than Wayland. What ships is mutool and the shared library, which
# is the half a scriptable machine wants.
MUPDF_SYS="USE_SYSTEM_LIBS=yes USE_SYSTEM_GUMBO=no USE_SYSTEM_MUJS=no \
	USE_SYSTEM_ZXINGCPP=no USE_SYSTEM_LCMS2=no"
MUPDF_OPT="tesseract=yes archive=yes barcode=no HAVE_LIBCRYPTO=yes \
	HAVE_X11=no HAVE_GLUT=no HAVE_OBJCOPY=yes"

# THE HYPHENATION ZIPS ARE REBUILT FROM THE TEXT PATTERNS beside them, so what
# is linked into libmupdf is made from source this archive carries rather than
# taken on trust as a binary. hyph-std.zip is core/, hyph-all.zip is core/ and
# extra/; entries go in path order under the fixed timestamp upstream's
# scripts/zip.py uses, so the result is reproducible. Deflate is what
# scripts/runhyphen.sh reaches through advzip, and mupdf's zip reader takes it.
# The hex dump under generated/ is removed with them: the objcopy path never
# reads it, and nothing prebuilt stays in the tree to be picked up instead.
rm -f resources/hyphen/hyph-std.zip resources/hyphen/hyph-all.zip \
	generated/resources/hyphen/*.zip.c
python3 - <<'PY'
import glob, zipfile
for out, dirs in (("hyph-std.zip", ("core",)), ("hyph-all.zip", ("core", "extra"))):
    files = sorted(f for d in dirs for f in glob.glob(f"resources/hyphen/{d}/*"))
    with zipfile.ZipFile(f"resources/hyphen/{out}", "w") as z:
        for f in files:
            info = zipfile.ZipInfo(f.rsplit("/", 1)[1], (1997, 8, 29, 2, 14, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(f, "rb") as src:
                z.writestr(info, src.read(), compresslevel=9)
PY

make $MUPDF_SYS $MUPDF_OPT build=release prefix=/usr shared=yes
make $MUPDF_SYS $MUPDF_OPT build=release prefix=/usr shared=yes \
	DESTDIR=$PKG install
