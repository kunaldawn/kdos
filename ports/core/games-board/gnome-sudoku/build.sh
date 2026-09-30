# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# Puzzles are generated and rated by qqwing; the interface is compiled from
# Blueprint files at build time.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release
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
	rsvg-convert -w $s -h $s data/application-icons/hicolor/scalable/org.gnome.Sudoku.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.Sudoku.png"
done

# UPSTREAM'S ENTRY IS REPLACED: the GtkApplication id is the Wayland app_id.
# DBusActivatable is dropped, so the launcher starts the program named in Exec.
cat > "$PKG/usr/share/applications/org.gnome.Sudoku.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Sudoku
GenericName=Sudoku Game
Comment=Test yourself in the classic puzzle
TryExec=gnome-sudoku
Exec=gnome-sudoku
Categories=GNOME;GTK;Game;LogicGame;
Keywords=sudoku;magic;square;puzzle;numbers;
Icon=org.gnome.Sudoku
Terminal=false
StartupNotify=true
StartupWMClass=org.gnome.Sudoku
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.gnome.Sudoku.desktop"
