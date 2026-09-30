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

# introspection installs the GstAudio, GstVideo, GstPbutils, GstGL and sibling
# GIRs, which gst-plugins-bad's GIRs and WebKitGTK's media code include.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Dintrospection=enabled \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dnls=disabled \
	-Dx11=disabled \
	-Dxshm=disabled \
	-Dxvideo=disabled \
	-Dxi=disabled \
	-Dqt5=disabled \
	-Dgl=enabled \
	-Dgl_api=opengl,gles2 \
	-Dgl_platform=egl \
	-Dgl_winsys=wayland,egl,surfaceless,gbm \
	-Dgl-jpeg=enabled \
	-Dgl-png=enabled \
	-Dgl-graphene=disabled \
	-Ddrm=enabled \
	-Dalsa=enabled \
	-Dogg=enabled \
	-Dvorbis=enabled \
	-Dopus=enabled \
	-Dpango=enabled \
	-Diso-codes=enabled \
	-Dorc=enabled \
	-Dorc-compiler=disabled \
	-Dtheora=disabled \
	-Dtremor=disabled \
	-Dcdparanoia=disabled \
	-Dlibvisual=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
