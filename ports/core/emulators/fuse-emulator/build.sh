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

# The SDL 2 front end, drawn by sdl2-compat on SDL 3's Wayland driver. The GTK
# front end would reach GDK's X11 backend and link libX11 for a keyboard
# grab that the SDL one does not need. Sound, the joystick and the frame
# timer all go through SDL; libxml2 keeps the settings file in fuse's own
# XML form; libpng is the screenshot writer. The desktop entry, the MIME
# types and the icons are upstream's, installed by --enable-desktop-
# integration. The Spectrum ROMs are in the tarball, distributed with
# Amstrad's permission, and are installed with the program.
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

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the SDL front end sets the
# Wayland app_id to net.sourceforge.fuse_emulator.Fuse. The MIME types it
# claims are the Spectrum formats its own fuse.xml defines.
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
