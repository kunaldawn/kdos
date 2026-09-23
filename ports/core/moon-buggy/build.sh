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

# NOT `make install`. The stock target also runs install-data-hook, which
# chgrp's the binary to `games` and sets it setgid so every player shares one
# high-score file. This image has one setgid binary's worth of appetite for
# that and it is spent elsewhere: the named targets install the program, its
# manual and its info page and nothing else, and the score file becomes the
# player's own under $HOME.
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
