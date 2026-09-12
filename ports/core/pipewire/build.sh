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

# -Dgstreamer=enabled builds the `pipewiresrc` element, which is the only way
# anything reads a PipeWire node from a pipeline: `kdos-record` drives the
# ScreenCast portal for a node id and hands it to gst-launch. Without it the
# element does not exist and the recorder has nothing to read the desktop with.
# The cost is that pipewire — which every image with sound installs — now pulls
# gstreamer and gst-plugins-base with it; this tree has no split packages, so
# the element and the audio server arrive together or not at all.

# -Dv4l2=enabled builds the SPA plugin that puts a camera on the graph, which
# is what media-session's `api.v4l2.*` mapping has been pointing at all along.
# -Dpipewire-v4l2 stays off: that half is an LD_PRELOAD shim which resolves its
# passthrough with dlsym(RTLD_NEXT, "openat64") and dlsym(RTLD_NEXT, "mmap64"),
# and musl exports no large-file aliases at all — both come back NULL and are
# called anyway, so every process started under it faults on its first open().

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
	-Dgstreamer=enabled \
	-Djack=disabled \
	-Dpipewire-jack=disabled \
	-Dpipewire-v4l2=disabled \
	-Dv4l2=enabled \
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
