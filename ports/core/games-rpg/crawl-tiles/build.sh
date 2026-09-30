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

cd source

# The tiles build, with sound. Every library is the system's: pkgconf finds
# SDL2, FreeType, SQLite and Lua 5.4, so nothing under contrib/ is built.
# The fonts are DejaVu's from ttf-dejavu, named here, rather than the copies
# under contrib/fonts. The map descriptions of other languages are left out
# (LANGUAGES empty). Saves go to ~/.crawl, the program in /usr/bin. The
# Makefile assigns CFLAGS and LDFLAGS outright, so the tree's flags reach it
# through EXTERNAL_FLAGS and EXTERNAL_LDFLAGS.
mk() {
	make \
		TILES=y SOUND=y \
		EXTERNAL_FLAGS="$CXXFLAGS" EXTERNAL_LDFLAGS="$LDFLAGS" \
		prefix=/usr bin_prefix=bin \
		NO_TRY_GOLD=y NO_TRY_LLD=y \
		LANGUAGES= \
		PROPORTIONAL_FONT=/usr/share/fonts/TTF/DejaVuSans.ttf \
		MONOSPACED_FONT=/usr/share/fonts/TTF/DejaVuSansMono.ttf \
		"$@"
}
mk
mk DESTDIR=$PKG install

for s in 32 48 512; do
	install -Dm644 dat/tiles/stone_soup_icon-${s}x$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x$s/apps/crawl-tiles.png"
done

# The Wayland app_id is SDL's default, the executable's name: crawl.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/crawl-tiles.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Dungeon Crawl Stone Soup
GenericName=Roguelike
Comment=Descend the Dungeon for the Orb of Zot, in tiles
Exec=crawl
Icon=crawl-tiles
Terminal=false
StartupWMClass=crawl
Categories=Game;AdventureGame;RolePlaying;
Keywords=roguelike;dungeon;crawl;dcss;stone soup;
DESKTOP
chmod 644 "$PKG/usr/share/applications/crawl-tiles.desktop"
