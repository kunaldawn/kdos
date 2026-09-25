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

# Every loader is a meson `feature` defaulting to `auto`, which builds it only
# if its library happens to be installed first; each is pinned so a missing
# library fails setup instead of silently dropping a format.
#
# highway is the SIMD path; orc is consulted only when highway is absent, so it
# is pinned off rather than left to be found.
#
# pdfium is looked up as `pdfium.pc`, and the pdfium port installs only
# `libpdfium.pc`; poppler is looked up as `poppler-glib`, which the poppler port
# does not build. Both PDF loaders are therefore off.
#
# cgif, libuhdr, matio, nifti, openslide, spng and quantizr are not ports.
#
# imagequant is GPL-3.0-or-later, so the libvips built here is GPL-3 as
# distributed.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dintrospection=disabled -Ddocs=false -Dcpp-docs=false -Dexamples=false \
	-Dmodules=disabled -Dcplusplus=true \
	-Djpeg=enabled -Dpng=enabled -Dtiff=enabled -Dwebp=enabled \
	-Dlcms=enabled -Dexif=enabled -Drsvg=enabled -Dpangocairo=enabled \
	-Dfontconfig=enabled -Dzlib=enabled -Dfftw=enabled -Darchive=enabled \
	-Dmagick=enabled -Draw=enabled -Dcfitsio=enabled -Dheif=enabled \
	-Dopenexr=enabled -Dopenjpeg=enabled -Djpeg-xl=enabled \
	-Dimagequant=enabled -Dhighway=enabled \
	-Dorc=disabled -Dpdfium=disabled -Dpoppler=disabled \
	-Dcgif=disabled -Duhdr=disabled -Dmatio=disabled -Dnifti=disabled \
	-Dopenslide=disabled -Dspng=disabled -Dquantizr=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
