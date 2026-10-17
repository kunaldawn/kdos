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

patch -p1 -i "$PORT_SRC/nullptr-def.patch"
patch -p1 -i "$PORT_SRC/x11-hints-on-x11-only.patch"

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
