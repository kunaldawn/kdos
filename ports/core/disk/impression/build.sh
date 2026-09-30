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

# meson drives cargo with its own CARGO_HOME under the build tree, so the
# vendor bundle's .cargo/config.toml is found the other way cargo looks for
# one: upward from the directory it runs in, which is build/ under this tree.
# Unpacked anywhere else, every crate resolves as missing. CARGO_NET_OFFLINE
# turns a crate the bundle lacks into an error instead of a download.
# native-tls links the system OpenSSL.
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
export CARGO_NET_OFFLINE=true
export OPENSSL_NO_VENDOR=1
export RUSTFLAGS="-C target-feature=-crt-static"

# Impression writes through the udisks2 daemon: it asks udisksd to open the
# drive, and polkit decides who may. Local image files are written with no
# network; its download list of distributions needs one.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dprofile=default
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only; GTK falls back to the source strings.
rm -rf "$PKG/usr/share/locale"

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s \
		data/resources/icons/hicolor/scalable/apps/io.gitlab.adhami3310.Impression.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/io.gitlab.adhami3310.Impression.png"
done

# UPSTREAM'S ENTRY IS REPLACED: the GtkApplication id is the Wayland app_id.
# MimeType is left out: disc images and xz archives are claimed by K3b, Ark
# and the archive opener, and mimeapps.list is where a default is chosen.
# DBusActivatable is dropped with it, so the launcher starts the program
# named in Exec.
cat > "$PKG/usr/share/applications/io.gitlab.adhami3310.Impression.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Impression
GenericName=Media Writer
Comment=Create bootable drives
TryExec=impression
Exec=impression %u
Icon=io.gitlab.adhami3310.Impression
Terminal=false
StartupNotify=true
StartupWMClass=io.gitlab.adhami3310.Impression
Categories=GTK;GNOME;Utility;System;DiscBurning;
Keywords=usb;flash;writer;bootable;drive;iso;img;disk;image;media;creator;live;
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.gitlab.adhami3310.Impression.desktop"
