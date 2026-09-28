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
# THE THREE EXCEPTIONS ARE NAMED, because USE_SYSTEM_LIBS=yes turns EVERY one
# of them on at once and a missing header is a build that dies a few files in.
# gumbo-parser and mujs are mupdf's own maintained forks with no upstream to be
# a port of, and lcms2mt is a forked variant of lcms2 that mupdf itself says is
# strongly preferred. The rest — freetype, harfbuzz, jbig2dec, libjpeg,
# openjpeg, zlib, curl, brotli, leptonica, tesseract, libarchive, zxing-cpp
# and freeglut — are ports and come from the system.
#
# barcode=yes is `mutool barcode`: decoding QR codes and barcodes out of a PDF
# page or an image, and drawing new ones, through zxing-cpp. It stops the build
# when zxing is missing, like the two below.
#
# tesseract=yes is OCR output (`mutool draw -F ocr.pdf`), reading tesseract's
# eng.traineddata; archive=yes opens cbr, cb7 and tar comic archives through
# libarchive. Both stop the build with an error when their library is missing.
# HAVE_LIBCRYPTO=yes is PDF signing and signature checks: left unset it is a
# pkg-config probe that drops them without a word when openssl is absent.
#
# BOTH VIEWERS ARE BUILT, AND BOTH ARE X11 CLIENTS that run under Xwayland.
# mupdf-gl is the full one — search, annotations, forms, a file dialog when
# started with no document — drawn through GLUT and GLX; HAVE_GLUT=yes makes
# a missing gl, x11, xrandr or glut a link error rather than a skipped viewer.
# Built against the freeglut port rather than mupdf's own fork, it has no
# clipboard and reads the keyboard as Latin-1 characters: both need the fork's
# GLUT API version 6, and freeglut declares 4. mupdf-x11 is the plain Xlib
# viewer, the lighter of the two. mutool and the shared library ship beside
# them.
MUPDF_SYS="USE_SYSTEM_LIBS=yes USE_SYSTEM_GUMBO=no USE_SYSTEM_MUJS=no \
	USE_SYSTEM_ZXINGCPP=yes USE_SYSTEM_LCMS2=no USE_SYSTEM_GLUT=yes"
MUPDF_OPT="tesseract=yes archive=yes barcode=yes HAVE_LIBCRYPTO=yes \
	HAVE_X11=yes HAVE_GLUT=yes HAVE_OBJCOPY=yes"

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

# The menu entry is mupdf-gl's. freeglut sets no WM_CLASS on its window, so
# there is no class for StartupWMClass to name. No MimeType: kdos-peek holds
# PDF, and mimeapps.list chooses the default. The icons are upstream's PNGs.
for s in 16 24 32 48 72 128 256 512; do
	install -Dm644 docs/logo/mupdf-icon-$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x$s/apps/mupdf.png"
done
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/mupdf-gl.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=PDF Viewer (MuPDF)
GenericName=Document Viewer
Comment=Read, search and annotate PDF, EPUB, XPS and comic book files
Exec=mupdf-gl %f
Icon=mupdf
Terminal=false
Categories=Office;Viewer;Graphics;
Keywords=pdf;epub;xps;cbz;document;reader;annotate;mupdf;
DESKTOP
chmod 644 "$PKG/usr/share/applications/mupdf-gl.desktop"
