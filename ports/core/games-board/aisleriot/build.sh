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
	-Dguile=3.0 \
	-Dtheme_svg_rsvg=true \
	-Dtheme_svg_qtsvg=false \
	-Dtheme_kde=false \
	-Dtheme_pysol=false \
	-Dtheme_fixed=true \
	-Dsound=true \
	-Dgconf=false \
	-Ddocs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

cat > "$PKG/usr/share/applications/sol.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=AisleRiot Solitaire
GenericName=Patience Card Games
Comment=Play many different solitaire games
TryExec=sol
Exec=sol
Icon=gnome-aisleriot
Terminal=false
StartupNotify=true
StartupWMClass=org.gnome.aisleriot
Categories=GNOME;GTK;Game;CardGame;
Keywords=solitaire;cards;klondike;spider;freecell;patience;aisleriot;
DESKTOP
chmod 644 "$PKG/usr/share/applications/sol.desktop"
