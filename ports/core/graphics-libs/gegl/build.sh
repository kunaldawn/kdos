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

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Ddocs=false \
	-Dgtk-doc=false \
	-Dgi-docgen=disabled \
	-Dworkshop=false \
	-Dintrospection=true \
	-Dvapigen=enabled \
	-Dparallel-tests=false \
	-Doperation-test=false \
	-Drelocatable-bundle=no \
	-Dgdk-pixbuf=enabled \
	-Dgexiv2=enabled \
	-Dgraphviz=disabled \
	-Djasper=disabled \
	-Dlcms=enabled \
	-Dlensfun=enabled \
	-Dlibav=disabled \
	-Dlibraw=enabled \
	-Dlibrsvg=enabled \
	-Dlibspiro=enabled \
	-Dlibtiff=enabled \
	-Dlibv4l=disabled \
	-Dlibv4l2=disabled \
	-Dlua=disabled \
	-Dmrg=disabled \
	-Dmaxflow=disabled \
	-Dopenexr=enabled \
	-Dopenmp=enabled \
	-Dcairo=enabled \
	-Dpango=enabled \
	-Dpangocairo=enabled \
	-Dpoppler=enabled \
	-Dpygobject=disabled \
	-Dsdl1=disabled \
	-Dsdl2=disabled \
	-Dsdl3=disabled \
	-Dumfpack=enabled \
	-Dwebp=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
test -e "$PKG/usr/lib/gegl-0.4/matting-levin.so"
readelf -d "$PKG/usr/bin/gegl" | grep -q 'NEEDED.*\[libspiro\.'
