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

# Every format option is a found-or-not probe that builds without the library
# when it is absent, so each is named true here and its library is in depends;
# a missing one still configures, and `meson setup`'s summary is where it
# shows. colord is not a port: without it the monitor profile is not read and
# images are shown untransformed for the display. GStreamer is a silent probe
# too: without it videos show only as file icons.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Ddeveloper-mode=false \
	-Dflatpak-build=false \
	-Dcolord=false \
	-Dlibwebp=true \
	-Dlibrsvg=true \
	-Dlibheif=true \
	-Dlibjxl=true \
	-Dlibtiff=true \
	-Dlibgif=true \
	-Dlibraw=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only, and English is the untranslated source text.
rm -rf "$PKG/usr/share/locale"

# The panel draws application icons from hicolor PNGs only, and upstream
# installs an SVG.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s -o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.gthumb.png" \
		data/icons/org.gnome.gthumb.svg
done

# The GApplication id is the Wayland app_id. No MimeType: the image types
# already have a handler on this desktop, and a second claim would make the
# default whichever entry sorts first.
cat > "$PKG/usr/share/applications/org.gnome.gthumb.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=gThumb
GenericName=Photo Browser
Comment=Browse, view, tag and edit photos and videos
Exec=gthumb %U
Icon=org.gnome.gthumb
Terminal=false
StartupNotify=true
StartupWMClass=org.gnome.gthumb
Categories=GTK;Graphics;Viewer;RasterGraphics;2DGraphics;Photography;
Keywords=image;photo;viewer;browser;thumbnail;catalog;gthumb;
Actions=new-window;

[Desktop Action new-window]
Name=New Window
Exec=gthumb --new-window
EOF
chmod 644 "$PKG/usr/share/applications/org.gnome.gthumb.desktop"
