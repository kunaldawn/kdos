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

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TRAINING_TOOLS=OFF \
	-DDISABLE_TIFF=OFF \
	-DDISABLE_ARCHIVE=OFF \
	-DDISABLE_CURL=OFF \
	-DGRAPHICS_DISABLED=ON \
	-DUSE_SYSTEM_ICU=ON
for def in HAVE_TIFFIO_H HAVE_LIBARCHIVE HAVE_LIBCURL; do
	grep -q "^#define $def " config_auto.h || { echo "tesseract: $def not configured" >&2; exit 1; }
done
ninja
DESTDIR=$PKG ninja install

mkdir -p man
for page in tesseract.1 unicharset.5; do
	asciidoctor -b manpage -o man/$page ../doc/$page.asc
	install -Dm644 man/$page -t "$PKG/usr/share/man/man${page##*.}"
done
