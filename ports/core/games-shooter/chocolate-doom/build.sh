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

autoreconf -fi

# Fonts and icons are regenerated from their PNGs, which needs Pillow.
./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man \
	--enable-sdl2mixer \
	--enable-sdl2net \
	--enable-doc \
	--enable-bash-completion \
	--enable-fonts \
	--enable-icons \
	--with-libsamplerate \
	--with-libpng \
	--with-fluidsynth \
	--disable-werror
make
make appdatadir=/usr/share/metainfo DESTDIR=$PKG install

# The menu carries Doom and the setup tool. Heretic, Hexen and Strife need
# game data KDOS does not ship, so their programs stay and their menu rows go.
# The window's app_id is the program's name.
rm -f "$PKG/usr/share/applications/org.chocolate_doom".{Heretic,Hexen,Strife}.desktop
cat > "$PKG/usr/share/applications/org.chocolate_doom.Doom.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Chocolate Doom
GenericName=First Person Shooter
Comment=Play Doom as it was on DOS
Exec=chocolate-doom
Icon=chocolate-doom
Terminal=false
StartupWMClass=chocolate-doom
Categories=Game;ActionGame;
Keywords=first;person;shooter;doom;freedoom;vanilla;
DESKTOP
cat > "$PKG/usr/share/applications/org.chocolate_doom.Setup.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Chocolate Doom Setup
Comment=Controls, sound, video and network settings for Chocolate Doom
Exec=chocolate-setup
Icon=chocolate-setup
Terminal=false
StartupWMClass=chocolate-setup
Categories=Settings;Game;
Keywords=doom;setup;controls;joystick;
DESKTOP
chmod 644 "$PKG/usr/share/applications/"*.desktop
