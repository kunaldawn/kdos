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

make \
	SDL_CONFIG='pkg-config sdl2' \
	DATADIR=/usr/share/brogue-ce \
	TERMINAL=NO GRAPHICS=YES RELEASE=YES

install -Dm755 bin/brogue "$PKG/usr/libexec/brogue-ce/brogue"
install -Dm644 bin/keymap.txt "$PKG/usr/share/brogue-ce/keymap.txt"
install -d "$PKG/usr/share/brogue-ce/assets"
install -m644 bin/assets/* "$PKG/usr/share/brogue-ce/assets/"
install -Dm644 bin/assets/icon.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/brogue-ce.png"
install -Dm644 LICENSE.txt "$PKG/usr/share/licenses/brogue-ce/LICENSE.txt"

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/brogue" <<'KDOS_SH'
#!/bin/sh
dir="${XDG_DATA_HOME:-$HOME/.local/share}/brogue-ce"
mkdir -p "$dir"
cd "$dir" || exit 1
exec /usr/libexec/brogue-ce/brogue "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/brogue"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/brogue-ce.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Brogue CE
GenericName=Roguelike
Comment=Descend 26 levels of the Dungeons of Doom for the Amulet of Yendor
Exec=brogue
Icon=brogue-ce
Terminal=false
StartupWMClass=brogue
Categories=Game;RolePlaying;
Keywords=roguelike;dungeon;brogue;rpg;
DESKTOP
chmod 644 "$PKG/usr/share/applications/brogue-ce.desktop"
