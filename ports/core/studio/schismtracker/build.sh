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

patch -p1 -i "$PORT_SRC/version.patch"
SCHISM_VERSION=$version autoreconf -fi

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

rm -f "$PKG/usr/share/pixmaps/schism-icon-128.png"
for s in 32 48 64 128 192; do
	install -Dm644 icons/schism-icon-$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/schismtracker.png"
done

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
