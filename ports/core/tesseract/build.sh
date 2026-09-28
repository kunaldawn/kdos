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

# THE ENGINE WITHOUT A LANGUAGE FILE DOES NOTHING, AND THAT IS THE TRAP. A
# tesseract install with no `.traineddata` starts, accepts an image and exits
# with "Failed loading language 'eng'" — which reads as a broken build. The
# models are the tessdata-eng port, a dependency of this one, which installs the
# English and script-detection models into /usr/share/tessdata; another
# language is one more file there.
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
# DISABLE_*=OFF ONLY MEANS "LOOK": a library that is not found turns its
# feature off and configure succeeds. CMAKE_REQUIRE_FIND_PACKAGE_CURL cannot
# pin curl: FindCURL first asks for a CMake package config, which the
# autotools-built curl does not install, and fails there before its pkg-config
# fallback runs. The generated header is the answer, so check it:
# without libarchive a compressed .traineddata will not load, without libcurl an
# image URL is not an input, and without TIFF a multi-page scan is not.
for def in HAVE_TIFFIO_H HAVE_LIBARCHIVE HAVE_LIBCURL; do
	grep -q "^#define $def " config_auto.h || { echo "tesseract: $def not configured" >&2; exit 1; }
done
ninja
DESTDIR=$PKG ninja install

# The CMake build installs no manual pages; upstream renders them only from its
# autotools build, with `asciidoctor -b manpage`, which is the command used here.
mkdir -p man
for page in tesseract.1 unicharset.5; do
	asciidoctor -b manpage -o man/$page ../doc/$page.asc
	install -Dm644 man/$page -t "$PKG/usr/share/man/man${page##*.}"
done
