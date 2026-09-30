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

# The three patches are Alpine's: flatbuffers.patch takes the bundled
# flatbuffers off strtod_l, which musl does not have; pathmax.patch gives the
# bundled zstd a PATH_MAX; make.patch drops cp's --no-preserve=ownership
# from the install rules.
patch -p1 -i "$PORT_SRC/flatbuffers.patch"
patch -p1 -i "$PORT_SRC/pathmax.patch"
patch -p1 -i "$PORT_SRC/make.patch"

# The tiles build with sound, dynamically linked against the system's SDL2
# libraries. No translations (LOCALIZE=0), no backtrace support (musl has
# no execinfo), no tests, no style checks. Saves and settings follow the
# XDG base directories. WARNINGS replaces upstream's list, which carries
# -Werror: a warning a newer compiler adds would otherwise stop the build.
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

# The panel reads PNG only: upstream's scalable icon is rasterised, and its
# entry replaced for StartupWMClass, the Wayland app_id being SDL's default,
# the executable's name.
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
