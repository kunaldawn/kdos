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

MUPDF_SYS="USE_SYSTEM_LIBS=yes USE_SYSTEM_GUMBO=no USE_SYSTEM_MUJS=no \
	USE_SYSTEM_ZXINGCPP=yes USE_SYSTEM_LCMS2=no USE_SYSTEM_GLUT=yes"
MUPDF_OPT="tesseract=yes archive=yes barcode=yes HAVE_LIBCRYPTO=yes \
	HAVE_X11=yes HAVE_GLUT=yes HAVE_OBJCOPY=yes"

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

export CC="gcc -std=gnu11" CFLAGS="${CFLAGS/-std=gnu11/}"
make $MUPDF_SYS $MUPDF_OPT build=release prefix=/usr shared=yes
make $MUPDF_SYS $MUPDF_OPT build=release prefix=/usr shared=yes \
	DESTDIR=$PKG install

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
