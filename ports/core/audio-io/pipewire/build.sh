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

export CFLAGS="$CFLAGS -frandom-seed=pipewire" CXXFLAGS="$CXXFLAGS -frandom-seed=pipewire"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Dbuildtype=release \
	-Db_lto=true \
	-Ddocs=disabled \
	-Dman=enabled \
	-Dtests=disabled \
	-Dexamples=disabled \
	-Dffmpeg=enabled \
	-Dpw-cat-ffmpeg=enabled \
	-Dalsa=enabled \
	-Dpipewire-alsa=enabled \
	-Dcompress-offload=enabled \
	-Dudev=enabled \
	-Ddbus=enabled \
	-Dlibusb=enabled \
	-Dselinux=disabled \
	-Dbluez5=enabled \
	-Dbluez5-codec-aptx=enabled \
	-Dbluez5-codec-ldac=enabled \
	-Dbluez5-codec-aac=enabled \
	-Dbluez5-codec-lc3=enabled \
	-Dbluez5-codec-opus=enabled \
	-Dbluez5-codec-g722=enabled \
	-Dbluez5-codec-lc3plus=disabled \
	-Dbluez5-codec-ldac-dec=disabled \
	-Dbluez5-plc-spandsp=disabled \
	-Dreadline=enabled \
	-Dlibpulse=enabled \
	-Dfftw=enabled \
	-Dopus=enabled \
	-Dgstreamer=enabled \
	-Dgstreamer-device-provider=enabled \
	-Djack=disabled \
	-Dpipewire-jack=enabled \
	-Djack-devel=true \
	-Dlibjack-path=/usr/lib \
	-Dpipewire-v4l2=disabled \
	-Dv4l2=enabled \
	-Dvulkan=disabled \
	-Droc=disabled \
	-Dlibcamera=enabled \
	-Dlv2=enabled \
	-Dsndfile=enabled \
	-Dpw-cat=enabled \
	-Davahi=enabled \
	-Draop=enabled \
	-Decho-cancel-webrtc=enabled \
	-Dlibmysofa=enabled \
	-Debur128=enabled \
	-Donnxruntime=disabled \
	-Dlibffado=disabled \
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
	-Dgsettings-pulse-schema=disabled \
	-Davb=enabled \
	-Dsnap=disabled \
	-Drlimits-install=false \
	"-Dsession-managers=[]"
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
