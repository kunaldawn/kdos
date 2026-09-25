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

# EVERY GUI BINDING IS OFF AND THAT IS THE HARD RULE, NOT A PREFERENCE.
# -DENABLE_QT5/QT6=OFF and -DENABLE_GLIB=OFF: there is no Qt and no GTK on this
# host, and the glib binding additionally drags cairo and gdk-pixbuf in for a
# renderer nothing here calls.
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
	-DENABLE_GLIB=OFF \
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
