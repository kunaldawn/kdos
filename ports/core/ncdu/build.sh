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

./configure --prefix=/usr
make
make DESTDIR=$PKG install

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
