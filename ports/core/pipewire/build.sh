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

# THE BLUETOOTH CODECS ARE NAMED RATHER THAN LEFT TO `auto`. Each one is a
# feature that quietly disables itself when its library is not found, so an
# `auto` build on a machine missing libfreeaptx produces a pipewire that
# negotiates SBC and says nothing about why — the exact failure the explicit
# `depends` line exists to prevent. SBC is mandatory in the profile and is
# always built; aptX, LDAC and AAC are what a headset actually asks for, and
# without them every device falls back to the worst codec in the spec.

# -Dv4l2=enabled builds the SPA plugin that puts a camera on the graph, which
# is what the session manager's `api.v4l2.*` mapping points at.
# -Dpipewire-v4l2 stays off: that half is an LD_PRELOAD shim which resolves its
# passthrough with dlsym(RTLD_NEXT, "openat64") and dlsym(RTLD_NEXT, "mmap64"),
# and musl exports no large-file aliases at all — both come back NULL and are
# called anyway, so every process started under it faults on its first open().

# THE LIMITS FILE IS NOT INSTALLED -- see -Drlimits-install below. Nothing on
# this image could read it and nobody could match it: limits.d is PAM's and
# shadow is built --without-libpam, so `login` links libc alone, while the match
# rule pipewire generates is `@pipewire`, a group that is not in /etc/group. A
# shipped grant that cannot fire is a claim the image does not honour, and it
# hides the one that does -- kdos-getty raises RLIMIT_RTPRIO, RLIMIT_NICE and
# RLIMIT_MEMLOCK as the last root process on either login path, and rlimits are
# inherited through setuid and execve to the session, to pipewire and to every
# ALSA client under them.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Dbuildtype=release \
	-Ddocs=disabled \
	-Dman=enabled \
	-Dtests=disabled \
	-Dexamples=disabled \
	-Dffmpeg=enabled \
	-Dbluez5=enabled \
	-Dbluez5-codec-aptx=enabled \
	-Dbluez5-codec-ldac=enabled \
	-Dbluez5-codec-aac=enabled \
	-Dbluez5-codec-lc3=enabled \
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
	-Dsndfile=enabled \
	-Dpw-cat=enabled \
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
	-Drlimits-install=false \
	"-Dsession-managers=[]"
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
