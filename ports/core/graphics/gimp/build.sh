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
	-Dcheck-update=no \
	-Drelocatable-bundle=no \
	-Denable-console-bin=true \
	-Denable-default-bin=enabled \
	-Denable-multiproc=true \
	-Dbash-completion=enabled \
	-Daa=enabled \
	-Dalsa=enabled \
	-Dappdata-test=disabled \
	-Dcairo-pdf=enabled \
	-Dfits=enabled \
	-Dghostscript=enabled \
	-Dgudev=enabled \
	-Dheif=enabled \
	-Dilbm=disabled \
	-Djpeg2000=enabled \
	-Djpeg-xl=enabled \
	-Dmng=disabled \
	-Dopenexr=enabled \
	-Dopenmp=enabled \
	-Dprint=true \
	-Dwebkit-unmaintained=false \
	-Dtwain-unmaintained=false \
	-Dwebp=enabled \
	-Dwmf=disabled \
	-Dxcursor=enabled \
	-Dxpm=enabled \
	-Dheadless-tests=disabled \
	-Dfile-plug-ins-test=false \
	-Dgi-docgen=disabled \
	-Dlinux-input=enabled \
	-Dvector-icons=true \
	-Dvala=disabled \
	-Djavascript=disabled \
	-Dlua=false \
	-Dlibunwind=true \
	-Dlibbacktrace=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
