# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# libgnome-games-support is a subproject the release carries and links
# statically; nodownload keeps meson on that copy.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only: GTK falls back to the source strings, and the
# help keeps its untranslated C pages.
rm -rf "$PKG/usr/share/locale"
if [ -d "$PKG/usr/share/help" ]; then
	find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +
fi

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/hicolor/scalable/org.gnome.Mines.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.Mines.png"
done

# UPSTREAM'S ENTRY IS REPLACED: the GtkApplication id is the Wayland app_id.
# DBusActivatable is dropped, so the launcher starts the program named in Exec.
cat > "$PKG/usr/share/applications/org.gnome.Mines.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Mines
GenericName=Minesweeper Game
Comment=Clear hidden mines from a minefield
TryExec=gnome-mines
Exec=gnome-mines
Categories=GNOME;GTK;Game;LogicGame;
Keywords=minesweeper;mines;puzzle;
Icon=org.gnome.Mines
Terminal=false
StartupNotify=true
StartupWMClass=org.gnome.Mines
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.gnome.Mines.desktop"
