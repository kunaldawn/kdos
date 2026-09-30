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

# FLTK picks Wayland or X11 at run time, and on Wayland fl_display is NULL:
# the Xlib window-hint call after the main window opens is made only when the
# X11 display is open.
patch -p1 -i "$PORT_SRC/x11-hints-on-x11-only.patch"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--with-flxmlrpc
make
make DESTDIR=$PKG install

# THE MENU: upstream's icon is XPM and its entry carries no window class; both
# are replaced. FLTK takes the Wayland app_id from the main window's xclass.
rm -f "$PKG/usr/share/applications/flmsg.desktop"
s=$(magick identify -format %w "data/flmsg.xpm")
install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
magick "data/flmsg.xpm" "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/flmsg.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/flmsg.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=flmsg
GenericName=Amateur Radio Message Forms
Comment=Compose and read ICS, Radiogram and other standard message forms
Exec=flmsg
Icon=flmsg
Terminal=false
StartupWMClass=flmsg
Categories=Network;HamRadio;
Keywords=ham;radio;ics;radiogram;message;forms;emcomm;
DESKTOP
chmod 644 "$PKG/usr/share/applications/flmsg.desktop"

test -x "$PKG/usr/bin/flmsg"
