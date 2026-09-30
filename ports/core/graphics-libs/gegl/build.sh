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

# libnsgif is the libnsgif port; poly2tri-c has no port and is built from the
# copy under subprojects/, which --wrap-mode=nodownload still allows.
#
# libav stays off, upstream's own default: the ff-load and ff-save operations
# are not used by GIMP and tie this library to every ffmpeg bump. umfpack stays
# off, so the matting-levin operation is absent and GIMP's foreground select
# uses matting-global. lua, mrg and the sdl options only feed the `gegl`
# viewer's interactive UI.
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
	-Dlibspiro=disabled \
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
	-Dumfpack=disabled \
	-Dwebp=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
