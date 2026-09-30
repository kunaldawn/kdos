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
	--with-zlib \
	--enable-truecolor \
	--disable-werror
make
make appdatadir=/usr/share/metainfo DESTDIR=$PKG install

# The menu carries Doom and the setup tool. Heretic, Hexen and Strife need
# game data KDOS does not ship, so their programs stay and their menu rows go.
# The window's app_id is the program's name.
rm -f "$PKG/usr/share/applications/io.github.fabiangreffrath".{Heretic,Hexen,Strife}.desktop
cat > "$PKG/usr/share/applications/io.github.fabiangreffrath.Doom.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Crispy Doom
GenericName=First Person Shooter
Comment=Play Doom in higher resolution, true to the original
Exec=crispy-doom
Icon=crispy-doom
Terminal=false
StartupWMClass=crispy-doom
Categories=Game;ActionGame;
Keywords=first;person;shooter;doom;freedoom;vanilla;
DESKTOP
cat > "$PKG/usr/share/applications/io.github.fabiangreffrath.Setup.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Crispy Doom Setup
Comment=Controls, sound, video and network settings for Crispy Doom
Exec=crispy-setup
Icon=crispy-setup
Terminal=false
StartupWMClass=crispy-setup
Categories=Settings;Game;
Keywords=doom;setup;controls;joystick;
DESKTOP
chmod 644 "$PKG/usr/share/applications/"*.desktop
