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

# The logbook table defines nullptr as a macro, which turns it into musl's
# NULL and breaks overload resolution.
patch -p1 -i "$PORT_SRC/nullptr-def.patch"
# FLTK picks Wayland or X11 at run time, and on Wayland fl_display is NULL:
# flarq's Xlib window-hint call is made only when the X11 display is open.
patch -p1 -i "$PORT_SRC/x11-hints-on-x11-only.patch"

# The bundled mbedtls copy is the 2.x API the network code is written
# against; the mbedtls port is 3.x, which has no net.h or certs.h. OSS is not
# on this system, and the manual is the asciidoc build, which is off with the
# rest of the docs.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-static \
	--disable-nls \
	--disable-oss \
	--enable-tls \
	--with-sndfile \
	--with-pulseaudio \
	--with-hamlib \
	--with-flxmlrpc \
	--without-libmbedtls \
	--without-asciidoc
make
make DESTDIR=$PKG install

# THE MENU: upstream's icons are XPM and its entries carry no window class;
# both are replaced. FLTK takes the Wayland app_id from the window's xclass,
# and both programs set it to the package name, fldigi: a flarq entry naming
# any other class would never match its own window.
rm -f "$PKG"/usr/share/applications/fldigi.desktop "$PKG"/usr/share/applications/flarq.desktop
for i in fldigi flarq; do
	s=$(magick identify -format %w "data/$i.xpm")
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	magick "data/$i.xpm" "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/$i.png"
done
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/fldigi.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=fldigi
GenericName=Amateur Radio Digital Modem
Comment=PSK31, RTTY, Olivia, MFSK, CW and other text modes through the sound card
Exec=fldigi
Icon=fldigi
Terminal=false
StartupWMClass=fldigi
Categories=Network;HamRadio;
Keywords=ham;radio;psk31;rtty;olivia;mfsk;cw;digital;modem;
DESKTOP
cat > "$PKG/usr/share/applications/flarq.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=flarq
GenericName=Amateur Radio ARQ File Transfer
Comment=Send files and messages error-free over an fldigi link
Exec=flarq
Icon=flarq
Terminal=false
StartupWMClass=fldigi
Categories=Network;HamRadio;
Keywords=ham;radio;arq;fldigi;file;transfer;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/fldigi.desktop "$PKG"/usr/share/applications/flarq.desktop

test -x "$PKG/usr/bin/fldigi"
test -x "$PKG/usr/bin/flarq"
