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

./autogen.sh

export XML_CATALOG_FILES=/etc/xml/catalog

./configure --prefix=/usr --sysconfdir=/etc --disable-nls --enable-docs
make doc_langs=
make DESTDIR=$PKG doc_langs= install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/calcurse.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Calendar (calcurse)
GenericName=Calendar and Todo
Comment=Appointments, todos and reminders
Exec=calcurse
Icon=x-office-calendar
Terminal=true
Categories=Office;Calendar;
Keywords=calendar;appointment;todo;reminder;calcurse;
EOF
chmod 644 "$PKG/usr/share/applications/calcurse.desktop"
