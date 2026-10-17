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

patch -p1 -i "$PORT_SRC/homebank-theme-files.patch"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-nls \
	--with-ofx
make
make DESTDIR=$PKG install
test -f "$PKG/usr/share/homebank/icons/Default/index.theme"
test -f "$PKG/usr/share/homebank/icons/hicolor/scalable/status/prf-transaction-transfer-symbolic.svg"

cat > "$PKG/usr/share/applications/homebank.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=HomeBank
GenericName=Personal finance
Comment=Free, easy, personal accounting for everyone
Icon=homebank
Exec=homebank %f
Terminal=false
MimeType=application/x-homebank;
Categories=Office;Finance;
Keywords=money;currency;business;finance;investment;budget;account;bank;statement;transaction;income;expense;bills;qif;csv;accounting;banking;
StartupNotify=true
StartupWMClass=fr.free.mdoyen.HomeBank
DESKTOP
chmod 644 "$PKG/usr/share/applications/homebank.desktop"
