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

# gparted is a shell wrapper that re-runs itself through pkexec, the command
# configure finds and writes in, and the action it asks for is
# org.gnome.gparted, which polkitd grants only by rule. pkexec keeps DISPLAY
# and XAUTHORITY for an allow_gui action but not WAYLAND_DISPLAY, so gpartedbin
# runs as root on GTK's X11 backend under Xwayland. --enable-xhost-root lets
# the wrapper grant root the display where an xhost command exists, and is a
# no-op where none does. The help needs a Yelp browser, which is not a port,
# so it is not built. Every filesystem the editor can create, grow, shrink or
# check is a separate command found at run time; the ones with ports are
# depended on, and GParted greys out the rest.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--libexecdir=/usr/libexec \
	--disable-static \
	--disable-doc \
	--enable-xhost-root
make
make DESTDIR=$PKG install

# Bundled data is English only; GTK falls back to the source strings.
rm -rf "$PKG/usr/share/locale"

# UPSTREAM'S ENTRY IS REPLACED. The window belongs to gpartedbin running as
# root on X11, and GTK names its class after the program, so the class is
# Gpartedbin. Exec is the bare wrapper, which asks polkit for root itself.
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
