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

# A GTK 3 burner straight over libburn and libisofs: no external writer is
# run, so it needs nothing beyond the libraries here. gudev is how it lists
# optical drives and notices a disc change; GStreamer decodes an audio
# composition's MP3, FLAC and Ogg files to CD audio. Both are required rather
# than probed.
#
# The manual page is built by xsltproc from DocBook with --nonet, so the
# stylesheet URL resolves through docbook-xsl's catalog.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dgudev=enabled \
	-Dgstreamer=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only; the interface falls back to its source strings.
rm -rf "$PKG/usr/share/locale"

# The Thunar "Send To" entry names a file manager that is not here.
rm -rf "$PKG/usr/share/Thunar"

# THE MENU ICON: upstream installs stock_xfburn under hicolor's stock/media
# context, which the panel's icon lookup does not read; the entry's icon is
# the same art in apps/, rasterised from the SVG at the sizes the panel asks
# for.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s icons/scalable/stock_xfburn.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/xfburn.png"
done

# UPSTREAM'S ENTRY IS REPLACED: GTK 3 takes the Wayland app_id from the program
# name, "xfburn", and the name is qualified because K3b is the burner the menu
# lists first.
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
