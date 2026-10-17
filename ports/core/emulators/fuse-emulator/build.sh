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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--with-sdl \
	--enable-sdl2 \
	--with-audio-driver=sdl \
	--with-joystick \
	--with-libxml2 \
	--with-png \
	--with-zlib \
	--enable-desktop-integration \
	--without-bash-completion-dir
make
make DESTDIR=$PKG install

_entry="$PKG/usr/share/applications/net.sourceforge.fuse_emulator.Fuse.desktop"
_mime=$(grep '^MimeType=' "$_entry")
cat > "$_entry" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Fuse
GenericName=ZX Spectrum Emulator
Comment=Emulate the ZX Spectrum home computer and its clones
TryExec=fuse
Exec=fuse %f
Icon=net.sourceforge.fuse_emulator.Fuse
Terminal=false
StartupWMClass=net.sourceforge.fuse_emulator.Fuse
Categories=Game;Emulator;
Keywords=sinclair;zx;spectrum;speccy;emulator;retro;tape;snapshot;
$_mime
DESKTOP
chmod 644 "$_entry"
