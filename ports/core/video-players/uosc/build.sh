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
export CGO_ENABLED=0
go build -mod=vendor -trimpath -ldflags "-s -w" -o ziggy-bin ./src/ziggy

install -d "$PKG/etc/mpv/scripts" "$PKG/etc/mpv/script-opts"
cp -r src/uosc "$PKG/etc/mpv/scripts/uosc"
rm -rf "$PKG/etc/mpv/scripts/uosc/intl"
install -Dm755 ziggy-bin "$PKG/usr/lib/uosc/ziggy"
install -d "$PKG/etc/mpv/scripts/uosc/bin"
ln -s /usr/lib/uosc/ziggy "$PKG/etc/mpv/scripts/uosc/bin/ziggy-linux"
install -Dm644 src/uosc.conf "$PKG/etc/mpv/script-opts/uosc.conf"

install -Dm644 -t "$PKG/usr/share/fonts/uosc" \
	src/fonts/uosc_icons.otf src/fonts/uosc_textures.ttf
install -Dm644 LICENSE.LGPL "$PKG/usr/share/licenses/uosc/LICENSE.LGPL"
