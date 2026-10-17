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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-nls \
	--disable-schemas-compile \
	--disable-introspection \
	--with-gtk \
	--with-python \
	--without-perl \
	--without-psiconv \
	--without-paradox \
	--without-gda
make
make DESTDIR=$PKG install

cat > "$PKG/usr/share/applications/org.gnumeric.gnumeric.desktop" <<'DESKTOP'
[Desktop Entry]
Version=1.0
Type=Application
Name=Gnumeric
GenericName=Spreadsheet
Comment=Calculation, Analysis, and Visualization of Information
Exec=gnumeric --name org.gnumeric.gnumeric %F
Icon=org.gnumeric.gnumeric
Terminal=false
StartupNotify=true
Categories=Office;Spreadsheet;Science;Math;GTK;
Keywords=Spreadsheet;statistics;excel;gnumeric;
MimeType=application/x-gnumeric;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.gnumeric.gnumeric.desktop"
