# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

if [ -d "$PKG/usr/share/help" ]; then
	find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +
fi

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/application-icons/hicolor/scalable/org.gnome.Sudoku.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.Sudoku.png"
done

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
