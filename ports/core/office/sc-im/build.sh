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

patch -p1 -i $PORT_SRC/wayland-clipboard.patch

cd src

export CFLAGS="$CFLAGS -Wno-error"

make prefix=/usr LUA_PKGNAME=lua CPPFLAGS=-g0
make prefix=/usr LUA_PKGNAME=lua THEMEDIR=/usr/share/sc-im/themes DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/sc-im.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Spreadsheet
GenericName=Spreadsheet
Comment=A spreadsheet on the grid
Exec=sc-im %f
Icon=x-office-spreadsheet
Terminal=true
Categories=Office;Spreadsheet;
Keywords=spreadsheet;cells;calc;sc-im;
EOF
chmod 644 "$PKG/usr/share/applications/sc-im.desktop"
