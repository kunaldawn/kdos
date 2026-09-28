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

# The tag archive carries no configure script; autoreconf writes it. The
# version comes from `git log` at that point, and with no repository the
# program reports an empty one.
autoreconf -fi

# SDL3 is the one video and audio backend: SDL2 here is sdl2-compat over the
# same SDL3, and 1.2 is not a port. ALSA is schism's own MIDI backend. JACK
# stays out. Every library is linked rather than dlopened, so the package's
# dependencies are the ones the loader sees. --without-x: the X11 code is a
# clipboard and keyboard path for an SDL X11 window, which this SDL3 never
# opens.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--without-x \
	--with-sdl3 --enable-sdl3-linking \
	--without-sdl2 --without-sdl12 \
	--with-alsa --enable-alsa-linking \
	--without-jack \
	--with-flac --enable-flac-linking \
	--with-avformat --enable-avformat-linking \
	--with-zlib --enable-zlib-linking \
	--with-bzip2 --enable-bzip2-linking \
	--with-lzma --enable-lzma-linking \
	--with-zstd --enable-zstd-linking \
	--disable-tests
make
make DESTDIR=$PKG install

# THE MENU ICON, from upstream's own PNG set, under the program's name.
rm -f "$PKG/usr/share/pixmaps/schism-icon-128.png"
for s in 32 48 64 128 192; do
	install -Dm644 icons/schism-icon-$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/schismtracker.png"
done

# UPSTREAM'S ENTRY IS REPLACED. SDL3 names the window after the program,
# schismtracker. MimeType is left out, since the music player owns module
# files; mimeapps.list chooses defaults.
rm -f "$PKG/usr/share/applications/schism.desktop"
cat > "$PKG/usr/share/applications/schismtracker.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Schism Tracker
GenericName=Music Tracker
Comment=Compose music in the Impulse Tracker style
TryExec=schismtracker
Exec=schismtracker %f
Icon=schismtracker
Terminal=false
StartupWMClass=schismtracker
Categories=AudioVideo;Audio;Midi;Sequencer;Music;
Keywords=impulse;tracker;module;it;s3m;xm;mod;midi;music;
DESKTOP
chmod 644 "$PKG/usr/share/applications/schismtracker.desktop"
