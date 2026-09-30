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

# The tag archive carries no configure script; upstream generates it.
./autogen.sh

# SDL 2 through sdl2-compat; the SDL 3 build asks for a newer SDL than the
# sdl3 port. X11 integration (the Xkb keyboard-layout lookup) is off: it
# reads SDL's X11 window, and sdl2-compat has none to give. OpenGL output
# through libglvnd, FreeType for the TrueType console, libpng screenshots,
# FFmpeg video capture, FluidSynth and ALSA MIDI, the bundled MUNT MT-32,
# SDL2_net for the modem and IPX, libslirp for user-mode NE2000 networking
# and libpcap for host pass-through. Each library is found or quietly
# dropped, so each is in depends.
./configure --prefix=/usr --libdir=/usr/lib \
	--enable-sdl2 \
	--disable-x11 \
	--enable-opengl \
	--enable-freetype \
	--enable-printer \
	--enable-avcodec \
	--enable-libfluidsynth \
	--enable-libslirp \
	--enable-alsa-midi \
	--enable-mt32 \
	--enable-screenshots \
	--enable-xbrz \
	--enable-dynamic-core \
	--enable-dynrec
make
make DESTDIR=$PKG install

# English only: the interface falls back to its built-in English strings.
find "$PKG/usr/share/dosbox-x/languages" -name '*.lng' ! -name 'en_US.lng' -delete

# The icon is installed as SVG only, which the panel never reads.
for _size in 32 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	rsvg-convert -w "$_size" -h "$_size" \
		-o "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/dosbox-x.png" \
		contrib/icons/dosbox-x.svg
done

# UPSTREAM'S ENTRY IS REPLACED: its StartupWMClass carries a trailing space
# that no app_id matches. SDL's Wayland app_id is the executable's name.
cat > "$PKG/usr/share/applications/com.dosbox_x.DOSBox-X.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=DOSBox-X
GenericName=DOS Emulator
Comment=Run DOS, Windows 3.x and 9x, and PC-98 software
TryExec=dosbox-x
Exec=dosbox-x
Icon=dosbox-x
Terminal=false
StartupWMClass=dosbox-x
Categories=Game;Emulator;
Keywords=dos;msdos;emulator;pcjr;tandy;pc98;pc-98;windows 3.1;windows 95;retro;
DESKTOP
chmod 644 "$PKG/usr/share/applications/com.dosbox_x.DOSBox-X.desktop"
