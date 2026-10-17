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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dgudev=enabled \
	-Dgstreamer=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm -rf "$PKG/usr/share/Thunar"

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s icons/scalable/stock_xfburn.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/xfburn.png"
done

cat > "$PKG/usr/share/applications/xfburn.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Disc Burner (Xfburn)
GenericName=Disc Burning
Comment=Burn data and audio CDs and DVDs, and ISO images
TryExec=xfburn
Exec=xfburn %F
Icon=xfburn
Terminal=false
StartupNotify=true
StartupWMClass=xfburn
Categories=GTK;DiscBurning;Utility;AudioVideo;
Keywords=burn;cd;dvd;iso;disc;write;xfburn;
Actions=BurnImage;

[Desktop Action BurnImage]
Name=Burn Image
Exec=xfburn -i %f
DESKTOP
chmod 644 "$PKG/usr/share/applications/xfburn.desktop"
