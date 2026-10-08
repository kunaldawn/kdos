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

# Alpine's patch: musl defines NULL as a pointer, which C++ will not cast to
# the integer window handle Glide64mk2 passes.
patch -p1 -i "$PORT_SRC/fix-null-usage.patch"

# Every makefile asks sdl2-config for SDL unless both SDL variables are given,
# and sdl2-config does not exist here, so pkgconf answers them.
#
# OpenGL is linked as libOpenGL, the vendor-neutral GL of libglvnd, not the
# GLX libGL. The context is SDL's, made through EGL on Wayland, so nothing
# here needs GLX. Glide64mk2 is the exception: it calls the ARB and EXT
# extension entry points by name, which libglvnd's libGL exports and
# libOpenGL does not, and musl binds every symbol when the plugin is loaded,
# so against libOpenGL the plugin does not load at all.
#
# The core's on-screen display (FreeType, GLU) and its Vulkan video
# extension are on. The SDL audio plugin's two resamplers, libsamplerate and
# speexdsp, are each found by a probe that drops the resampler when the
# library is missing, which depends prevents. Netplay needs SDL2_net and a
# server, and is left at its default, off.
#
# The core includes <unzip.h> and takes minizip's flags from pkg-config, whose
# file names the include directory above minizip/, as its other consumers
# write <minizip/unzip.h>; MINIZIP_CFLAGS names the minizip/ directory itself.
# Given either MINIZIP variable, the makefile asks pkg-config for neither, so
# MINIZIP_LDLIBS is given too.
_mk=(
	PREFIX=/usr
	LIBDIR=/usr/lib
	PLUGINDIR=/usr/lib/mupen64plus
	SHAREDIR=/usr/share/mupen64plus
	SDL_CFLAGS="$(pkg-config --cflags sdl2)"
	SDL_LDLIBS="$(pkg-config --libs sdl2)"
	GL_CFLAGS=
	MINIZIP_CFLAGS="-I$(pkg-config --variable=includedir minizip)/minizip"
	MINIZIP_LDLIBS="$(pkg-config --libs minizip)"
	OSD=1
	VULKAN=1
	NETPLAY=0
	HIRES=1
	PIE=1
	V=1
)
_gl() { [ "$1" = video-glide64mk2 ] && echo -lGL || echo -lOpenGL; }
for _c in core ui-console audio-sdl input-sdl rsp-hle video-rice video-glide64mk2; do
	make -C "source/mupen64plus-$_c/projects/unix" all "${_mk[@]}" \
		GL_LDLIBS="$(_gl $_c)"
done
for _c in core ui-console audio-sdl input-sdl rsp-hle video-rice video-glide64mk2; do
	make -C "source/mupen64plus-$_c/projects/unix" install "${_mk[@]}" \
		GL_LDLIBS="$(_gl $_c)" LDCONFIG=true DESTDIR="$PKG"
done

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the core's SDL window
# takes the executable's name as its Wayland app_id. It stays NoDisplay: the
# console front end has no ROM browser and is opened with a ROM file.
cat > "$PKG/usr/share/applications/mupen64plus.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Mupen64Plus
GenericName=Nintendo 64 Emulator
Comment=Play a Nintendo 64 ROM image
TryExec=mupen64plus
Exec=mupen64plus %f
Icon=mupen64plus
Terminal=false
NoDisplay=true
StartupWMClass=mupen64plus
MimeType=application/x-n64-rom;
Categories=Game;Emulator;
Keywords=emulator;nintendo;n64;mupen64plus;
DESKTOP
chmod 644 "$PKG/usr/share/applications/mupen64plus.desktop"
