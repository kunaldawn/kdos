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

# -Dwith-coroutine=gthread: musl has no ucontext functions. The default probes
# for them and falls back on its own, but naming the backend keeps the choice
# from depending on what the probe happens to find.
# -Dpulseaudio=enabled is the audio bridge for VNC servers that send sound
# (QEMU's audio extension); it talks to pipewire-pulse.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dintrospection=enabled \
	-Dwith-vala=enabled \
	-Dpulseaudio=enabled \
	-Dsasl=enabled \
	-Dwith-coroutine=gthread \
	-Dgi-docs=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
