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

./configure --prefix=/usr \
            --mandir=/usr/share/man \
            --infodir=/usr/share/info \
            --sharedstatedir=/var/lib
make

make DESTDIR=$PKG install-exec install-data-local install-man install-info-am

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/$name.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Moon Buggy
GenericName=Arcade Game
Comment=Drive a buggy across the moon, jumping craters and shooting meteors
Exec=moon-buggy
Icon=input-gaming
Terminal=true
Categories=Game;ArcadeGame;
Keywords=game;arcade;moon;buggy;jump;
ENTRY
chmod 644 "$PKG/usr/share/applications/$name.desktop"
