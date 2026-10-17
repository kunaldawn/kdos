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

export CFLAGS="$CFLAGS -g0"

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
Exec=crispy-doom-setup
Icon=crispy-setup
Terminal=false
StartupWMClass=crispy-doom-setup
Categories=Settings;Game;
Keywords=doom;setup;controls;joystick;
DESKTOP
chmod 644 "$PKG/usr/share/applications/"*.desktop
