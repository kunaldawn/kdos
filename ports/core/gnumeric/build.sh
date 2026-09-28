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


# The Python plugin loader is built only when pygobject-3.0 is found, and the
# functions and plugins written in Python are absent without it. Psiconv,
# Paradox and libgda have no port, so those importers and the database plugin
# are off; the Perl loader carries no plugin anyone needs and is off with
# them. --disable-nls leaves out every translation: bundled data is English
# only. The GSettings schemas are compiled by kpkg's shared-index step, not
# at install into $PKG.
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

# The --name is the Wayland app_id. Only Gnumeric's own format is claimed: the
# spreadsheet types belong to LibreOffice Calc, and a second claim would make
# the default whichever entry sorts first.
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
