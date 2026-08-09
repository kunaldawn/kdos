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

# Vendor media-session subproject for offline build (wrap-git → directory)
ln -sf "$SRC_ROOT/media-session-master" subprojects/media-session

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Dbuildtype=release \
	-Ddocs=disabled \
	-Dman=disabled \
	-Dtests=disabled \
	-Dexamples=disabled \
	-Dffmpeg=enabled \
	-Dbluez5=enabled \
	-Dreadline=enabled \
	-Dlibpulse=disabled \
	-Dfftw=disabled \
	-Dopus=disabled \
	-Dgstreamer=disabled \
	-Djack=disabled \
	-Dpipewire-jack=disabled \
	-Dpipewire-v4l2=disabled \
	-Dv4l2=disabled \
	-Dvulkan=disabled \
	-Droc=disabled \
	-Dlibcamera=disabled \
	-Dlv2=disabled \
	-Dsndfile=disabled \
	-Davahi=disabled \
	-Dlibsystemd=disabled \
	-Dlogind=disabled \
	-Dsystemd-system-service=disabled \
	-Dsystemd-user-service=disabled \
	-Dsdl2=disabled \
	-Dx11=disabled \
	-Dx11-xfixes=disabled \
	-Dlibcanberra=disabled \
	-Dflatpak=disabled \
	-Dgsettings=disabled \
	-Dsnap=disabled \
	"-Dsession-managers=['media-session']"
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
