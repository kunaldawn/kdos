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

# The graphical front end only: the terminal one would duplicate every other
# roguelike on the console, and this port exists for the tiles. `sdl2-config`
# does not exist here, so the makefile's SDL_CONFIG asks pkgconf instead.
# DATADIR is compiled in; RELEASE=YES keeps the version string from asking git.
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

# Brogue writes its saves, recordings and high scores into the working
# directory. Started from the menu that is $HOME, or / for a launcher with no
# directory of its own, so the command changes into a directory of the
# player's before the game starts.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/brogue" <<'KDOS_SH'
#!/bin/sh
dir="${XDG_DATA_HOME:-$HOME/.local/share}/brogue-ce"
mkdir -p "$dir"
cd "$dir" || exit 1
exec /usr/libexec/brogue-ce/brogue "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/brogue"

# The Wayland app_id is SDL's default, the executable's name: brogue.
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
