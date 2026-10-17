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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--libexecdir=/usr/libexec \
	--disable-static \
	--disable-doc \
	--enable-xhost-root
make
make DESTDIR=$PKG install

cat > "$PKG/usr/share/applications/gparted.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=GParted
GenericName=Partition Editor
Comment=Create, reorganize, and delete partitions
TryExec=gparted
Exec=gparted %f
Icon=gparted
Terminal=false
StartupNotify=true
StartupWMClass=Gpartedbin
Categories=GTK;GNOME;System;Filesystem;
Keywords=partition;disk;format;resize;filesystem;gparted;
DESKTOP
chmod 644 "$PKG/usr/share/applications/gparted.desktop"
