# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# Every game is a Guile Scheme module, compiled with guild at build time and
# loaded by the same Guile at run time. The card faces are SVG drawn through
# librsvg; prerendered themes are read too, where a user installs one. The KDE
# and QtSVG card themes need Qt 5, so they are off; PySolFC themes need
# PySolFC's cards, which this image does not carry. Sound plays through
# libcanberra-gtk3.
#
# The help is Mallard for Yelp, which this image does not have, so it is not
# built and the Help item does nothing.
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

# Bundled data is English only: GTK falls back to the source strings.
rm -rf "$PKG/usr/share/locale"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the GtkApplication id is
# the Wayland app_id. The hicolor PNGs it names are upstream's, installed
# above.
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
