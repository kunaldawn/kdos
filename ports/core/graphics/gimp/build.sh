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

# Offline: check-update=no never asks gimp.org for a newer release.
#
# Every file-format and device feature that has a port is named on, so a
# missing library fails the setup instead of quietly dropping a loader. mng,
# wmf and ilbm have no port (libmng, libwmf, libilbm) and are off. The X11
# half is required because GTK 3 is built with its X11 backend: GIMP then
# links libXmu, libXext and libXfixes for its X11 window picking, and
# xcursor and xpm are the X cursor and XPM file plug-ins. aa is the ASCII-art
# exporter, alsa and gudev the MIDI and input-device controllers,
# linux-input the evdev controller, and ghostscript the PostScript loader
# through libgs.
#
# The Python plug-ins are built, so the setup needs PyGObject with pycairo
# and the GExiv2 typelib. JavaScript (gjs), Lua and Vala plug-ins are off:
# gjs and lua-lgi are not ports, and Vala has no plug-in here that needs it.
# libunwind is the nongnu one, which carries the pkg-config file the setup
# looks for; libbacktrace is not a port, so the dashboard backtraces are
# partially detailed. The unmaintained WebKit help browser and 32-bit TWAIN
# stay off; F1 opens gimp-help's local manual, under
# /usr/share/gimp/3.0/help, in the web browser.
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

# meson has no switch for the catalogues; bundled data is English only, and
# GIMP falls back to its source strings.
rm -rf "$PKG/usr/share/locale"
