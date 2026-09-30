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
	--buildtype=release \
	-Dprofile=default
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
	rsvg-convert -w $s -h $s data/icons/hicolor/scalable/org.gnome.Mahjongg.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.Mahjongg.png"
done

# UPSTREAM'S ENTRY IS REPLACED: the GtkApplication id is the Wayland app_id.
# DBusActivatable is dropped, so the launcher starts the program named in Exec.
cat > "$PKG/usr/share/applications/org.gnome.Mahjongg.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Mahjongg
GenericName=Mahjongg Solitaire
Comment=Match tiles and clear the board
TryExec=gnome-mahjongg
Exec=gnome-mahjongg
Categories=GNOME;GTK;Game;BoardGame;LogicGame;
Keywords=mahjong;mahjongg;tiles;solitaire;puzzle;
Icon=org.gnome.Mahjongg
Terminal=false
StartupNotify=true
StartupWMClass=org.gnome.Mahjongg
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.gnome.Mahjongg.desktop"
