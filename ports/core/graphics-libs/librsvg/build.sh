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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export CARGO_NET_OFFLINE=true
export RUSTFLAGS="-C target-feature=-crt-static"
# -Dpixbuf-loader=enabled installs the SVG loader into gdk-pixbuf's module
# directory, which is what lets every gdk-pixbuf consumer on the host open an
# SVG: the portal's icon validation, img2sixel, fcitx5's theme images. Under
# DESTDIR the install only prints a note instead of writing loaders.cache;
# kpkg's pixbuf trigger regenerates the cache when the package lands.
# introspection installs Rsvg-2.0, which PyGObject programs load; its GIR
# includes GdkPixbuf-2.0. The vapi, librsvg-2.0, is what Vala programs such as
# gnome-mines compile against; without it their build stops at valac.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dintrospection=enabled \
	-Dvala=enabled \
	-Ddocs=enabled \
	-Dtests=false \
	-Dpixbuf=enabled \
	-Dpixbuf-loader=enabled \
	-Davif=enabled \
	-Dtriplet=x86_64-unknown-linux-musl
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
