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

export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE"
make \
	WITH_SDL3=yes \
	WITH_OPENAL=yes \
	WITH_CURL=yes \
	WITH_EXECINFO=no \
	WITH_RPATH=no \
	WITH_XDG=yes \
	WITH_SYSTEMWIDE=yes \
	WITH_SYSTEMDIR=/usr/share/games/quake2

install -d "$PKG/usr/lib/yamagi-quake2/baseq2" "$PKG/usr/bin" \
	"$PKG/usr/share/games/quake2/baseq2"
install -m755 release/quake2 release/q2ded "$PKG/usr/lib/yamagi-quake2/"
install -m755 release/ref_*.so "$PKG/usr/lib/yamagi-quake2/"
install -m755 release/baseq2/game.so "$PKG/usr/lib/yamagi-quake2/baseq2/"
install -m644 stuff/yq2.cfg "$PKG/usr/share/games/quake2/baseq2/yq2.cfg"
ln -s ../lib/yamagi-quake2/quake2 "$PKG/usr/bin/quake2"
ln -s ../lib/yamagi-quake2/q2ded "$PKG/usr/bin/q2ded"
install -Dm644 stuff/icon/Quake2.png \
	"$PKG/usr/share/icons/hicolor/512x512/apps/yamagi-quake2.png"
install -Dm644 README.md CHANGELOG doc/*.md -t "$PKG/usr/share/doc/yamagi-quake2"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/yamagi-quake2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Yamagi Quake II
GenericName=First Person Shooter
Comment=Play Quake II; needs the game data in /usr/share/games/quake2/baseq2
Exec=quake2
Icon=yamagi-quake2
Terminal=false
StartupWMClass=quake2
Categories=Game;ActionGame;
Keywords=first;person;shooter;quake;quake2;
DESKTOP
chmod 644 "$PKG/usr/share/applications/yamagi-quake2.desktop"
