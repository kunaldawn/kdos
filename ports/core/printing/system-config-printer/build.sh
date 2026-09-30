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

# The USB auto-configuration helper (udev rules plus a systemd unit) is off:
# the session configures queues through cups-browsed and this program.
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--mandir=/usr/share/man \
	--without-udev-rules \
	--without-systemdsystemunitdir \
	--with-xmlto
make
make DESTDIR=$PKG install

# Bundled data is English only, and English is the untranslated source text.
rm -rf "$PKG/usr/share/locale"

# The applet is a GtkStatusIcon, which draws only on an X11 tray; autostarted
# it would sit in every session with no icon to show.
rm -f "$PKG/etc/xdg/autostart/print-applet.desktop"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the program calls
# GLib.set_prgname("system-config-printer"), which is its Wayland app_id.
# Icon=printer is drawn from the devices context of the KDOS icon atlas.
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
