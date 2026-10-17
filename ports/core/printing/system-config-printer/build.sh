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

patch -p1 -i "$PORT_SRC/gettext-1.0.patch"
patch -p1 -i "$PORT_SRC/makefile-am-fix-setup-py-install.patch"
autoreconf -fi

./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--mandir=/usr/share/man \
	--without-udev-rules \
	--without-systemdsystemunitdir \
	--with-xmlto
make
make DESTDIR=$PKG install

rm -f "$PKG/etc/xdg/autostart/print-applet.desktop"

cat > "$PKG/usr/share/applications/system-config-printer.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Print Settings
GenericName=Printer Configuration
Comment=Add printers and change their settings
Exec=system-config-printer
Icon=printer
Terminal=false
StartupNotify=true
StartupWMClass=system-config-printer
Categories=GTK;Settings;HardwareSettings;Printing;System;
Keywords=printer;print;queue;cups;driver;ppd;
DESKTOP
chmod 644 "$PKG/usr/share/applications/system-config-printer.desktop"
