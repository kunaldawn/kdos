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

# Every plugin that links an outside library is named, `enabled` or `disabled`,
# never left at meson's `auto`: an `enabled` library that goes missing fails
# the build instead of producing a silently thinner package, and a `disabled`
# one stays off whatever else happens to be installed first.
#
#   x265       HEVC encode          -Dgpl=enabled is required with it
#   faad       AAC decode           -Dgpl=enabled is required with it
#   svtav1     AV1 encode           (AV1 decode is gst-libav's, through ffmpeg)
#   fdkaac     AAC encode
#   openjpeg   JPEG 2000
#   assrender  SSA/ASS subtitles over video
#   va, kms, wayland, vulkan, gl    hardware decode/encode and the video sinks
#   webrtcdsp  echo cancellation and noise suppression
#   webrtc     webrtcbin, peer-to-peer calls: ICE is libnice, the media is
#              SRTP from libsrtp, the keys come from dtls and the data
#              channels from sctp, which carries its own usrsctp
#   srtp       srtpenc and srtpdec, which webrtcbin needs
#   resindvd   rsndvdbin, DVD playback with menus    -Dgpl=enabled as well
#   rsvg       SVG decode and SVG overlays on video
#   sndfile    sfdec, the formats libsndfile reads (upstream builds no encoder)
#   openexr    OpenEXR decode
#   bz2        bzip2 decode and encode
#
# aom is off: svt-av1 is this tree's AV1 encoder, and a second encoder for one
# format earns nothing. The Bluetooth codecs are off because PipeWire owns
# Bluetooth audio, and libde265 because gst-libav already decodes HEVC.
# srt stays off until srt is a port.
# introspection stays off: every GStreamer GIR includes Gst-1.0 and the
# GstBase GIRs, and gstreamer and gst-plugins-base are built without them.

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dnls=disabled \
	-Dintrospection=disabled \
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
	-Dsrt=disabled \
	-Davtp=disabled \
	-Dbs2b=disabled \
	-Dchromaprint=disabled \
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
	-Dopenmpt=disabled \
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
