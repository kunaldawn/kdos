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

# X11 IS OFF: there is no X server on this image, so an X11 video driver could
# never open a display. Wayland is the one video driver built.
#
# KMSDRM IS OFF, SO SDL FAILS CLEANLY INSTEAD OF WINNING. A KMSDRM program takes
# the card and the input devices from the compositor already drawing on them,
# leaving a desk nobody can get back to without a terminal switch. With the
# driver absent, a program started outside kdos-comp says "No available video
# device" and exits.
#
# AUDIO NEEDS NO VIDEO DEVICE. `SDL_Init(SDL_INIT_AUDIO)` opens no window, so
# whisper.cpp's streaming transcription runs in a terminal with nothing above
# it.
#
# PIPEWIRE IS THE AUDIO BACKEND, AND PULSE AND ALSA ARE BEHIND IT. The pulse
# backend is dlopened from libpulse and speaks to pipewire-pulse, for a
# program that asks for SDL_AUDIO_DRIVER=pulseaudio; ALSA stays as the floor
# underneath, for a machine with the session's sound server not running.
#
# HIDAPI OVER libusb is named because it is otherwise on only when libusb.h
# happens to be installed, and hidraw alone misses the Xbox, PlayStation and
# Nintendo pads that driver talks to directly. liburing backs SDL's async file
# I/O; without it the same API runs on a thread pool.
#
# sdl2-compat dlopens libSDL3.so.0 from the default search path, so the library
# lands in /usr/lib with no rpath.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSDL_SHARED=ON \
	-DSDL_STATIC=OFF \
	-DSDL_TEST_LIBRARY=OFF \
	-DSDL_TESTS=OFF \
	-DSDL_EXAMPLES=OFF \
	-DSDL_RPATH=OFF \
	-DSDL_X11=OFF \
	-DSDL_KMSDRM=OFF \
	-DSDL_WAYLAND=ON \
	-DSDL_WAYLAND_LIBDECOR=ON \
	-DSDL_ALSA=ON \
	-DSDL_PIPEWIRE=ON \
	-DSDL_PULSEAUDIO=ON \
	-DSDL_JACK=OFF \
	-DSDL_SNDIO=OFF \
	-DSDL_OSS=OFF \
	-DSDL_DBUS=ON \
	-DSDL_IBUS=OFF \
	-DSDL_LIBUDEV=ON \
	-DSDL_HIDAPI_LIBUSB=ON \
	-DSDL_LIBURING=ON \
	-DSDL_OPENGL=ON \
	-DSDL_OPENGLES=ON \
	-DSDL_VULKAN=ON \
	-DSDL_INSTALL_DOCS=ON
ninja
DESTDIR=$PKG ninja install
