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

# fix_amd_npu_libc_type is Alpine's: musl's ioctl() takes an int request, not
# an unsigned long, and the AMD NPU query does not compile without it.
patch -p1 -i "$PORT_SRC/fix_amd_npu_libc_type.patch"

# meson drives cargo twice, for the program and for its process_data helper,
# each with CARGO_HOME under the build tree, so the vendor bundle's
# .cargo/config.toml is found the other way cargo looks for one: upward from
# build/ under this tree. One bundle serves both lock files (vendorsync).
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
export CARGO_NET_OFFLINE=true
export RUSTFLAGS="-C target-feature=-crt-static"

# profile=default is the release build with the plain application id; the
# upstream default is the development profile, a debug build named
# net.nokyan.Resources.Devel. Ending another user's process and reading
# memory modules (dmidecode) go through pkexec, which polkit answers.
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
	rsvg-convert -w $s -h $s data/icons/net.nokyan.Resources.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/net.nokyan.Resources.png"
done

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the GtkApplication id is
# the Wayland app_id. The name is qualified: Resources is kdos-res's entry.
cat > "$PKG/usr/share/applications/net.nokyan.Resources.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Resources (GNOME)
GenericName=System Monitor
Comment=Keep an eye on system resources
Exec=resources
Icon=net.nokyan.Resources
Terminal=false
StartupNotify=true
StartupWMClass=net.nokyan.Resources
Categories=GTK;System;Monitor;
Keywords=system;resources;monitor;processes;usage;task;manager;cpu;ram;memory;gpu;npu;performance;power;battery;
DESKTOP
chmod 644 "$PKG/usr/share/applications/net.nokyan.Resources.desktop"
