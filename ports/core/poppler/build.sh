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

# THE QT6 BINDING IS ON: poppler-qt6 is the PDF backend of Okular, gImageReader,
# TeXstudio, LabPlot and Cantor, and Krita's PDF import filter. Qt6Test is one
# of the Qt modules poppler asks for even with its Qt6 tests off, and qt6-qtbase
# provides it. The Qt5 binding stays off: nothing here is built against Qt 5.
#
# THE GLIB BINDING IS ON, and it needs glib and cairo and nothing else: GTK is
# looked for only to build a demo, and gdk-pixbuf is never linked. poppler-glib
# is how timg shows a PDF in a terminal, and its Poppler typelib is what a
# PyGObject program such as PDF Arranger imports. Both the binding and the
# introspection data are optional in poppler's CMake, which turns either off
# without an error when its dependency is not found, so the checks after the
# install are what make a missing one a failed build. The gtk-doc reference
# stays off.
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
# BUILD_TESTING is not a poppler option; BUILD_CPP_TESTS, BUILD_MANUAL_TESTS
# and one BUILD_<binding>_TESTS per binding (GTK, QT6) are what gate its test
# programs, and every one defaults to ON.
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
	-DENABLE_GOBJECT_INTROSPECTION=ON \
	-DENABLE_GTK_DOC=OFF \
	-DBUILD_GTK_TESTS=OFF \
	-DENABLE_UNSTABLE_API_ABI_HEADERS=ON \
	-DENABLE_QT5=OFF \
	-DENABLE_QT6=ON \
	-DBUILD_QT6_TESTS=OFF \
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
[ -f "$PKG/usr/lib/girepository-1.0/Poppler-0.18.typelib" ] || {
	echo 'poppler: the Poppler typelib was not built; PyGObject programs could not import it' >&2
	exit 1
}
[ -f "$PKG/usr/lib/pkgconfig/poppler-qt6.pc" ] || {
	echo 'poppler: the Qt6 binding was not built; Okular and the other Qt readers need it' >&2
	exit 1
}
