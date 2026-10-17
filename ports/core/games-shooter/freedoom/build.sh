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

make VERSION=v$version

export XML_CATALOG_FILES=/etc/xml/catalog
make -C dist freedoom1.6 freedoom2.6 freedm.6 \
	io.github.freedoom.Phase1.png io.github.freedoom.Phase2.png \
	io.github.freedoom.FreeDM.png

install -Dm644 wads/freedoom1.wad wads/freedoom2.wad wads/freedm.wad \
	-t "$PKG/usr/share/games/doom"
install -Dm755 dist/freedoom "$PKG/usr/bin/freedoom1"
install -Dm755 dist/freedoom "$PKG/usr/bin/freedoom2"
install -Dm755 dist/freedoom "$PKG/usr/bin/freedm"
install -Dm644 dist/freedoom1.6 dist/freedoom2.6 dist/freedm.6 \
	-t "$PKG/usr/share/man/man6"
for i in Phase1 Phase2 FreeDM; do
	install -Dm644 dist/io.github.freedoom.$i.png \
		"$PKG/usr/share/icons/hicolor/64x64/apps/io.github.freedoom.$i.png"
done
install -Dm644 CREDITS CREDITS-MUSIC README.adoc NEWS.adoc COPYING.adoc \
	-t "$PKG/usr/share/doc/freedoom"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/io.github.freedoom.Phase1.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Freedoom: Phase 1
GenericName=First Person Shooter
Comment=Battle monsters in four 9-level episodes
Exec=freedoom1
Icon=io.github.freedoom.Phase1
Terminal=false
StartupWMClass=crispy-doom
Categories=Game;ActionGame;
Keywords=first;person;shooter;doom;freedoom;
DESKTOP
cat > "$PKG/usr/share/applications/io.github.freedoom.Phase2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Freedoom: Phase 2
GenericName=First Person Shooter
Comment=Battle monsters in a 32-level campaign
Exec=freedoom2
Icon=io.github.freedoom.Phase2
Terminal=false
StartupWMClass=crispy-doom
Categories=Game;ActionGame;
Keywords=first;person;shooter;doom;freedoom;
DESKTOP
chmod 644 "$PKG/usr/share/applications/"*.desktop
