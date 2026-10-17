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
	--buildtype=release \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dnls=disabled \
	-Dintrospection=enabled \
	-Dx11=disabled \
	-Dgtk3=disabled \
	-Dorc=enabled \
	-Dorc-compiler=disabled \
	-Dgpl=enabled \
	-Dx265=enabled \
	-Dfaad=enabled \
	-Dsvtav1=enabled \
	-Dfdkaac=enabled \
	-Dopenjpeg=enabled \
	-Dassrender=enabled \
	-Dopus=enabled \
	-Dwebp=enabled \
	-Dzbar=enabled \
	-Dfluidsynth=enabled \
	-Dcolormanagement=enabled \
	-Danalyticsoverlay=enabled \
	-Dwebrtcdsp=enabled \
	-Dwebrtc=enabled \
	-Dsrtp=enabled \
	-Dsctp=enabled \
	-Dsctp-internal-usrsctp=enabled \
	-Dresindvd=enabled \
	-Drsvg=enabled \
	-Dsndfile=enabled \
	-Dopenexr=enabled \
	-Dbz2=enabled \
	-Dchromaprint=enabled \
	-Dopenmpt=enabled \
	-Dva=enabled \
	-Dudev=enabled \
	-Ddrm=enabled \
	-Dkms=enabled \
	-Dwayland=enabled \
	-Dgl=enabled \
	-Dvulkan=enabled \
	-Dvulkan-video=enabled \
	-Dvulkan-windowing=wayland \
	-Ddash=enabled \
	-Dhls=enabled \
	-Dhls-crypto=openssl \
	-Dsmoothstreaming=enabled \
	-Dttml=enabled \
	-Dcurl=enabled \
	-Dcurl-ssh2=enabled \
	-Ddtls=enabled \
	-Daes=enabled \
	-Daom=disabled \
	-Dbluez=disabled \
	-Dsbc=disabled \
	-Dldac=disabled \
	-Dopenaptx=disabled \
	-Dlc3=disabled \
	-Dlibde265=disabled \
	-Dcodec2json=disabled \
	-Dqroverlay=disabled \
	-Dv4l2codecs=disabled \
	-Duvch264=disabled \
	-Duvcgadget=disabled \
	-Dsrt=enabled \
	-Davtp=disabled \
	-Dbs2b=disabled \
	-Ddc1394=disabled \
	-Ddirectfb=disabled \
	-Ddts=disabled \
	-Dfaac=disabled \
	-Dflite=disabled \
	-Dgme=disabled \
	-Dgs=disabled \
	-Dgsm=disabled \
	-Diqa=disabled \
	-Disac=disabled \
	-Dladspa=disabled \
	-Dladspa-rdf=disabled \
	-Dlcevcdecoder=disabled \
	-Dlcevcencoder=disabled \
	-Dlv2=disabled \
	-Dmicrodns=disabled \
	-Dmodplug=disabled \
	-Dmpeg2enc=disabled \
	-Dmpeghdec=disabled \
	-Dmplex=disabled \
	-Dmsdk=disabled \
	-Dmusepack=disabled \
	-Dneon=disabled \
	-Dnvcomp=disabled \
	-Dnvdswrapper=disabled \
	-Donnx=disabled \
	-Dopenal=disabled \
	-Dopencv=disabled \
	-Dopenh264=disabled \
	-Dopenni2=disabled \
	-Drtmp=disabled \
	-Dsoundtouch=disabled \
	-Dspandsp=disabled \
	-Dsvthevcenc=disabled \
	-Dsvtjpegxs=disabled \
	-Dteletext=disabled \
	-Dtflite=disabled \
	-Dtflite-edgetpu=disabled \
	-Dtinyalsa=disabled \
	-Dvmaf=disabled \
	-Dvoaacenc=disabled \
	-Dvoamrwbenc=disabled \
	-Dwildmidi=disabled \
	-Dwpe=disabled \
	-Dwpe2=disabled \
	-Dzxing=disabled \
	-Daja=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
