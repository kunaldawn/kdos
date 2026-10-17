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
