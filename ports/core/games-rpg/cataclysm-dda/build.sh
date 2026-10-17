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

patch -p1 -i "$PORT_SRC/flatbuffers.patch"
patch -p1 -i "$PORT_SRC/pathmax.patch"
patch -p1 -i "$PORT_SRC/make.patch"
patch -p1 -i "$PORT_SRC/zzip-ldflags.patch"

mk() {
	make \
		PREFIX=/usr \
		WARNINGS='-Wall -Wextra' \
		RELEASE=1 TILES=1 SOUND=1 \
		LOCALIZE=0 LANGUAGES= \
		USE_XDG_DIR=1 DYNAMIC_LINKING=1 \
		BACKTRACE=0 PCH=0 \
		TESTS=0 RUNTESTS=0 \
		ASTYLE=0 LINTJSON=0 \
		"$@"
}
mk
mk DESTDIR=$PKG install

rm -rf "$PKG/usr/share/icons/hicolor/scalable"
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 data/xdg/org.cataclysmdda.CataclysmDDA.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/org.cataclysmdda.CataclysmDDA.png"
rm -f "$PKG/usr/share/applications/org.cataclysmdda.CataclysmDDA.desktop"
cat > "$PKG/usr/share/applications/org.cataclysmdda.CataclysmDDA.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Cataclysm: Dark Days Ahead
GenericName=Survival Game
Comment=Turn-based survival in a post-apocalyptic world
Exec=cataclysm-tiles
Icon=org.cataclysmdda.CataclysmDDA
Terminal=false
StartupWMClass=cataclysm-tiles
Categories=Game;RolePlaying;
Keywords=zombie;survival;roguelike;tiles;dda;cdda;cataclysm;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.cataclysmdda.CataclysmDDA.desktop"
