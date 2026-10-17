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

patch -p1 -i "$PORT_SRC/atspi-no-extern-c.patch"
patch -p1 -i "$PORT_SRC/x11-backend.patch"

printf '%s' "$version" > .tarball-version
autoreconf -fi

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--enable-atspi \
	--enable-speech=speechdispatcher \
	--with-gsettings \
	--with-cairo \
	--without-gnome \
	--disable-japanese \
	--disable-joystick \
	--disable-tilt
make
make DESTDIR=$PKG install

cat > "$PKG/usr/share/applications/dasher.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Dasher
GenericName=Predictive Text Entry
Comment=Write without a keyboard, by pointer, eye tracker or switch
TryExec=dasher
Exec=dasher
Icon=dasher
Terminal=false
StartupNotify=true
StartupWMClass=dasher
Categories=GTK;Utility;Accessibility;
Keywords=text entry;keyboard;eye tracking;switch;pointer;accessibility;a11y;dasher;
DESKTOP
chmod 644 "$PKG/usr/share/applications/dasher.desktop"
