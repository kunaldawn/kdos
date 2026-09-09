# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# -std=gnu17 IS THE WHOLE OF THE PORT. This is 2006 C: signal.c declares its
# handler argument as `RETSIGTYPE (*handler) ()`, an empty parameter list, which
# C17 reads as "unspecified" and C23 reads as `(void)`. Under the compiler's
# new default every install_signal() call is a hard error on an incompatible
# pointer type, where it was a warning for twenty years.
#
# A FLAG AND NOT A PATCH, which is the rule: the code is correct under the
# standard it was written to, and a patch here would be this tree carrying a
# rewrite of somebody else's signal handling for the rest of time.
export CFLAGS="$CFLAGS -std=gnu17"

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
