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


# libofx has no port, so OFX and QFX statements do not import; QIF and CSV
# do. libsoup is REQUIRED by configure and is used only by the currency-rate
# update, which fails offline and leaves the rates as entered. --disable-nls
# leaves out every translation: bundled data is English only.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-nls \
	--without-ofx
make
make DESTDIR=$PKG install

# The window's Wayland app_id is its GApplication id, which is neither the
# entry's file name nor its last component, so the entry names it in
# StartupWMClass; without it the taskbar cannot match the window to the
# Start menu row.
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
