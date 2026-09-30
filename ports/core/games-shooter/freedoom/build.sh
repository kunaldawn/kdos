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

# The IWADs are assembled here from the PNGs, sounds, music and maps in the
# tree. VERSION would otherwise come from `git describe`, and this is an
# archive with no repository, so the version lump would read "unknown".
make VERSION=v$version

# Icons and manual pages. a2x resolves docbook-xsl's stylesheet only through
# the XML catalog. The PDF manuals need asciidoctor-pdf, which is not a port.
export XML_CATALOG_FILES=/etc/xml/catalog
make -C dist freedoom1.6 freedoom2.6 freedm.6 \
	io.github.freedoom.Phase1.png io.github.freedoom.Phase2.png \
	io.github.freedoom.FreeDM.png

# Every Doom engine here searches /usr/share/games/doom for freedoom1.wad and
# freedoom2.wad. The launcher runs the first engine it finds from its own list,
# which puts crispy-doom ahead of chocolate-doom, so that is the window class.
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

# FreeDM holds deathmatch maps only, so it has a command and no menu row.
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
