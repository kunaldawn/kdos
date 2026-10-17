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

export CFLAGS="$CFLAGS -std=gnu23"
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

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s -o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.gthumb.png" \
		data/icons/org.gnome.gthumb.svg
done

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
