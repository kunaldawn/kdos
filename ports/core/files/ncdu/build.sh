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

export ZIG_GLOBAL_CACHE_DIR="$SRC_ROOT/zig-cache"
mkdir -p "$ZIG_GLOBAL_CACHE_DIR/p"

unpack_zig_dep() {
	mkdir -p "$ZIG_GLOBAL_CACHE_DIR/p/$2"
	tar xf "$PORT_SRC/ncdudep-$1.tar.gz" --strip-components=1 \
		-C "$ZIG_GLOBAL_CACHE_DIR/p/$2"
}
unpack_zig_dep translate_c translate_c-0.0.0-Q_BUWgpXBwDWbTJQO9Ul01ZN4NJ4CEqQ4ZX1vLrcy38W
unpack_zig_dep aro         aro-0.0.0-JSD1QrExOgBF8ySFOvEaZfP1LBTjgwg58Y7UHoGJ3nOd

zig build --system "$ZIG_GLOBAL_CACHE_DIR/p" -fsys=ncurses -fsys=zstd \
	-Doptimize=ReleaseFast -Dcpu=baseline -Dpie=true -Dstrip=true \
	--prefix "$PKG/usr"
install -Dm644 ncdu.1 "$PKG/usr/share/man/man1/ncdu.1"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ncdu.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Disk Usage
GenericName=Disk Usage Analyser
Comment=Find what is filling the disk
Exec=ncdu
Icon=drive-harddisk
Terminal=true
Categories=System;Filesystem;
Keywords=disk;usage;space;du;ncdu;
EOF
chmod 644 "$PKG/usr/share/applications/ncdu.desktop"
