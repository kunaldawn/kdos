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

./autogen.sh

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

find "$PKG/usr/share/dosbox-x/languages" -name '*.lng' ! -name 'en_US.lng' -delete

for _size in 32 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	rsvg-convert -w "$_size" -h "$_size" \
		-o "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/dosbox-x.png" \
		contrib/icons/dosbox-x.svg
done

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
