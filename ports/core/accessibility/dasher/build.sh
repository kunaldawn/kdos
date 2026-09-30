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

# Dasher's GTK 3 front end reads the pointer position with X11 calls, so
# x11-backend.patch pins GDK to the X11 backend and the window runs under
# Xwayland; on the Wayland backend it does not steer. Direct mode reads the
# focused window's AT-SPI text and types through at-spi2-core's X11 keyboard
# path, so it reaches only Xwayland windows that publish AT-SPI text.
# atspi-no-extern-c.patch includes the AT-SPI header as C++: glib's headers
# carry C++ templates and fail to compile inside extern "C".
patch -p1 -i "$PORT_SRC/atspi-no-extern-c.patch"
patch -p1 -i "$PORT_SRC/x11-backend.patch"

# The release version comes from git describe or from .tarball-version, and a
# forge archive carries neither; without one AC_INIT has no version.
printf '%s' "$version" > .tarball-version
autoreconf -fi

# Direct mode types into other applications through AT-SPI (atspi-2), not
# XTest. Speech is speech-dispatcher, preferences are GSettings. The GNOME
# help is off: it needs yelp-tools, which is not a port.
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

# Bundled data is English only: GTK falls back to the source strings. The
# training texts and alphabets for other languages stay: they are what a
# person picks to write in that language, not translations of the interface.
rm -rf "$PKG/usr/share/locale"

# UPSTREAM'S ENTRY IS REPLACED: it carries GNOME bug-tracker keys, and the
# window's X11 class is the program name. The menu icon is the 48x48 PNG
# upstream installs in hicolor.
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
