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

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dintrospection=disabled -Ddocs=false -Dcpp-docs=false -Dexamples=false \
	-Dmodules=disabled -Dcplusplus=true \
	-Djpeg=enabled -Dpng=enabled -Dtiff=enabled -Dwebp=enabled \
	-Dlcms=enabled -Dexif=enabled -Drsvg=enabled -Dpangocairo=enabled \
	-Dfontconfig=enabled -Dzlib=enabled -Dfftw=enabled -Darchive=enabled \
	-Dmagick=enabled -Draw=enabled -Dcfitsio=enabled -Dheif=enabled \
	-Dopenexr=enabled -Dopenjpeg=enabled -Djpeg-xl=enabled \
	-Dimagequant=enabled -Dhighway=enabled \
	-Dorc=disabled -Dpdfium=enabled -Dpoppler=disabled \
	-Dcgif=disabled -Duhdr=disabled -Dmatio=enabled -Dnifti=disabled \
	-Dopenslide=disabled -Dspng=disabled -Dquantizr=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
