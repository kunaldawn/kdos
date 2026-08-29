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
# -DENABLE_BOOST=OFF drops the splash renderer's optional SIMD path rather than
# adding boost to a library whose consumers here are two command-line tools.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DENABLE_UTILS=ON \
	-DENABLE_CPP=ON \
	-DENABLE_GLIB=OFF \
	-DENABLE_QT5=OFF \
	-DENABLE_QT6=OFF \
	-DENABLE_BOOST=OFF \
	-DENABLE_GPGME=OFF \
	-DENABLE_LIBCURL=OFF \
	-DENABLE_NSS3=OFF \
	-DENABLE_LCMS=ON \
	-DENABLE_LIBOPENJPEG=openjpeg2 \
	-DENABLE_DCTDECODER=libjpeg \
	-DWITH_NSS3=OFF
ninja
DESTDIR=$PKG ninja install
