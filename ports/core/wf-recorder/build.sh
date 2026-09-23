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

# AUDIO IS PIPEWIRE'S, NAMED. Both backends are `auto` and pulse wins the
# default whenever libpulse-simple is installed, so left alone the recorder's
# audio path follows whatever the chroot happens to hold. Enabled, a missing
# libpipewire stops configure instead of producing a recorder with no audio.
#
# libavdevice has no switch — it is a `required: false` probe — and is found
# because ffmpeg builds it; it is what writes to a v4l2loopback device.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dpipewire=enabled \
	-Dpulse=disabled \
	-Ddefault_audio_backend=pipewire
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
