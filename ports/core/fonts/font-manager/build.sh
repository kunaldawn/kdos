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

# Vala 0.56.19 refuses the cast the 0.9.4 sources put on Gtk.DragIcon.
patch -p1 -i "$PORT_SRC/vala-0.56.19.patch"

# webkit=false removes the Google Fonts catalogue, the one part that needs the
# network, and with it WebKitGTK. search-provider is a GNOME Shell interface
# no process here reads. yelp-doc needs yelp-tools and a help viewer, neither
# of which is a port. enable-nls=false keeps the catalogues out: bundled data
# is English only. The file-manager extensions are for Nautilus, Nemo and
# Thunar. reproducible=true keeps the build date out of the binaries.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dmanager=true \
	-Dviewer=true \
	-Dsearch-provider=false \
	-Dadwaita=false \
	-Dwebkit=false \
	-Dlibarchive=true \
	-Dyelp-doc=false \
	-Denable-nls=false \
	-Dunihan=true \
	-Dnautilus=false \
	-Dnemo=false \
	-Dthunar=false \
	-Dgtk-doc=false \
	-Dreproducible=true \
	-Dapp-armor=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# The panel draws application icons from hicolor PNGs only, and upstream
# installs one SVG per program.
for id in com.github.FontManager.FontManager com.github.FontManager.FontViewer; do
	for s in 48 64 128 256; do
		install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
		rsvg-convert -w $s -h $s -o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/$id.png" \
			data/com.github.FontManager.FontManager.svg
	done
done

# The GApplication ids are the Wayland app_ids, and each entry names its own.
# Only the viewer claims the font types, so opening a font file has one
# handler and not a choice between two windows of the same package. The types
# are the MIME database's canonical names: the desktop resolves a file to one
# of those from its glob, so an alias or an unknown name never matches.
cat > "$PKG/usr/share/applications/com.github.FontManager.FontManager.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Font Manager
GenericName=Font Manager
Comment=Browse, compare, enable and disable installed fonts
Exec=font-manager %u
Icon=com.github.FontManager.FontManager
Terminal=false
StartupNotify=true
StartupWMClass=com.github.FontManager.FontManager
Categories=GTK;Utility;Graphics;
Keywords=font;typeface;glyph;character;preview;compare;
EOF
cat > "$PKG/usr/share/applications/com.github.FontManager.FontViewer.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Font Viewer
GenericName=Font Viewer
Comment=Preview a font file and install it for this user
Exec=/usr/libexec/font-manager/font-viewer %u
Icon=com.github.FontManager.FontViewer
Terminal=false
StartupNotify=true
StartupWMClass=com.github.FontManager.FontViewer
MimeType=font/ttf;font/otf;font/collection;
Categories=GTK;Utility;Graphics;
Keywords=font;typeface;glyph;preview;install;
EOF
chmod 644 "$PKG/usr/share/applications/com.github.FontManager.FontManager.desktop" \
	"$PKG/usr/share/applications/com.github.FontManager.FontViewer.desktop"
