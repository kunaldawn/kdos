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

# THE QT BINDINGS ARE OFF BY RULE: there is no Qt on this host.
#
# THE GLIB BINDING IS ON, and it needs glib and cairo and nothing else: GTK is
# looked for only to build a demo, and gdk-pixbuf is never linked. poppler-glib
# is how timg shows a PDF in a terminal. The binding is optional in poppler's
# CMake, which turns it off without an error when glib is not found, so the
# check after the install is what makes a missing poppler-glib a failed build.
# Introspection stays off, because nothing on the host loads a Poppler
# typelib, and so does the gtk-doc reference.
#
# -DENABLE_UNSTABLE_API_ABI_HEADERS=ON installs the private xpdf headers GDAL's
# PDF driver compiles against. They carry no stability promise: a poppler
# version bump has to be test-built with gdal too, because its PDF driver may no
# longer compile against the new headers.
#
# WHAT IS ACTUALLY WANTED IS pdftotext. recoll's PDF filter shells out to it,
# and without it recollindex walks a directory of PDFs, reports success and
# produces an index with nothing in it — a silent failure, which is why the
# utils are the point of this port rather than a side effect. -DENABLE_UTILS=ON
# is therefore load-bearing.
#
# -DENABLE_BOOST=ON uses boost's headers for the Splash rasteriser's small
# containers; nothing links boost at run time.
#
# pdfsig verifies and signs through the GPG backend (gpgmepp). The NSS backend
# stays off because nss is not a port.
#
# BUILD_TESTING is not a poppler option; BUILD_CPP_TESTS and BUILD_MANUAL_TESTS
# are what gate its test programs, and both default to ON.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_CPP_TESTS=OFF \
	-DBUILD_MANUAL_TESTS=OFF \
	-DENABLE_UTILS=ON \
	-DENABLE_CPP=ON \
	-DENABLE_GLIB=ON \
	-DENABLE_GOBJECT_INTROSPECTION=OFF \
	-DENABLE_GTK_DOC=OFF \
	-DBUILD_GTK_TESTS=OFF \
	-DENABLE_UNSTABLE_API_ABI_HEADERS=ON \
	-DENABLE_QT5=OFF \
	-DENABLE_QT6=OFF \
	-DENABLE_BOOST=ON \
	-DENABLE_GPGME=ON \
	-DENABLE_LIBCURL=OFF \
	-DENABLE_NSS3=OFF \
	-DENABLE_LCMS=ON \
	-DENABLE_LIBJPEG=ON \
	-DENABLE_LIBTIFF=ON \
	-DENABLE_HARFBUZZ=ON \
	-DWITH_Cairo=ON \
	-DWITH_PNG=ON \
	-DENABLE_LIBOPENJPEG=openjpeg2
ninja
DESTDIR=$PKG ninja install
[ -f "$PKG/usr/lib/pkgconfig/poppler-glib.pc" ] || {
	echo 'poppler: the glib binding was not built; timg would lose PDF support' >&2
	exit 1
}
