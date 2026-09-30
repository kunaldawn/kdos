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
rm -f "$PKG/usr/share/applications/flamp.desktop"
s=$(magick identify -format %w "data/flamp.xpm")
install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
magick "data/flamp.xpm" "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/flamp.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/flamp.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=flamp
GenericName=Amateur Multicast File Transfer
Comment=Send files to many stations at once over an fldigi link
Exec=flamp
Icon=flamp
Terminal=false
StartupWMClass=flamp
Categories=Network;HamRadio;
Keywords=ham;radio;amp;multicast;file;transfer;fldigi;
DESKTOP
chmod 644 "$PKG/usr/share/applications/flamp.desktop"

test -x "$PKG/usr/bin/flamp"
