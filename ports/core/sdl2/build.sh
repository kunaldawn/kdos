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

# THE 2 LINE AND NOT THE 3 LINE, and that is what the consumers decide. SDL3 is
# upstream's current one; baresip's `sdl` video display and whisper.cpp's
# streaming example both look for `sdl2` through pkg-config and neither has an
# SDL3 path, so an SDL3 here would be a library nothing on this image links.
#
# X11 IS OFF AND IT IS NOT A SIZE DECISION. There is no X server on this image
# by rule, so an X11 video driver is a driver that cannot open a display and a
# set of client libraries the rule refuses. Wayland is the one video driver
# built.
#
# KMSDRM IS OFF, AND WITHOUT IT SDL FAILS CLEANLY INSTEAD OF WINNING. A KMSDRM
# program takes the card and the input devices directly — which on this image
# means taking them from the compositor that is already drawing on them, leaving
# a desk nobody can get back to without a terminal switch. A graphical program
# here runs under kdos-comp. With the driver absent, a program started anywhere
# else says "No available video device" and exits, which is a sentence somebody
# can act on.
#
# AND AUDIO IS THE HALF WITH NO VIDEO DEVICE AT ALL. `SDL_Init(SDL_INIT_AUDIO)`
# opens no window, so whisper.cpp's streaming transcription runs in a terminal
# with nothing above it — which is the one consumer here that is not a window.
#
# PULSE IS OFF AND PIPEWIRE IS ON: PipeWire is this image's server and there is
# no PulseAudio port, so the pulse backend would be a dlopen that never finds
# its library. ALSA stays as the floor underneath, for a machine with the
# session's sound server not running.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSDL_SHARED=ON \
	-DSDL_STATIC=OFF \
	-DSDL_TEST=OFF \
	-DSDL_RPATH=OFF \
	-DSDL_X11=OFF \
	-DSDL_KMSDRM=OFF \
	-DSDL_WAYLAND=ON \
	-DSDL_WAYLAND_LIBDECOR=ON \
	-DSDL_ALSA=ON \
	-DSDL_PIPEWIRE=ON \
	-DSDL_PULSEAUDIO=OFF \
	-DSDL_JACK=OFF \
	-DSDL_SNDIO=OFF \
	-DSDL_ESD=OFF \
	-DSDL_OSS=OFF \
	-DSDL_DBUS=ON \
	-DSDL_IBUS=OFF \
	-DSDL_LIBUDEV=ON \
	-DSDL_OPENGL=ON \
	-DSDL_OPENGLES=ON \
	-DSDL_VULKAN=ON
ninja
DESTDIR=$PKG ninja install

# THE CONFIG SCRIPT GOES AND THE pkg-config FILE STAYS. `sdl2-config` hardcodes
# the prefix it was built with and is what a consumer reaches for when
# pkg-config cannot answer — so leaving it means a build that finds the wrong
# flags silently rather than failing at the point the answer was missing.
rm -f "$PKG/usr/bin/sdl2-config"
