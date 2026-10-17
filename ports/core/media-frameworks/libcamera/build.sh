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
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--wrap-mode=nodownload \
	-Dbuildtype=release \
	-Dwerror=false \
	-Dpipelines=ipu3,simple,uvcvideo \
	-Dipas=ipu3,simple \
	-Dudev=enabled \
	-Dgstreamer=enabled \
	-Dcam=enabled \
	-Dcam-output-kms=enabled \
	-Dcam-output-sdl2=disabled \
	-Dcam-jpeg=disabled \
	-Dapps-output-dng=disabled \
	-Dlc-compliance=disabled \
	-Dqcam=disabled \
	-Dpycamera=disabled \
	-Dv4l2=disabled \
	-Dandroid=disabled \
	-Ddocumentation=disabled \
	-Dtracing=disabled \
	-Dsoftisp-gpu=disabled \
	-Drpi-awb-nn=disabled \
	-Dlibdw=disabled \
	-Dlibunwind=enabled \
	-Dtest=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
