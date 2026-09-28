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

# WITH_X11=OFF: remote input goes through libei on Wayland; the X11 path
# needs libfakekey. BLUETOOTH_ENABLED=ON builds the Bluetooth link provider
# on QtBluetooth from qt6-qtconnectivity, which reaches bluetoothd over
# D-Bus; the LAN provider is built either way.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_INSTALL_LIBEXECDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D WITH_X11=OFF \
	-D WITH_PULSEAUDIO=ON \
	-D BLUETOOTH_ENABLED=ON \
	-D LOOPBACK_ENABLED=OFF \
	-D INSTALL_UFW_APPLICATION_RULE=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6ModemManagerQt=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The panel draws only PNG icons, and upstream installs the logo as SVG.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s icons/app/sc-apps-kdeconnect.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/kdeconnect.png"
done

# KAboutData names each window's app_id from the reversed organisation
# domain and the component name, which StartupWMClass must repeat.
_apps="$PKG/usr/share/applications"
cat > "$_apps/org.kde.kdeconnect.app.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KDE Connect
GenericName=Device Synchronization
Comment=Share files, clipboard and notifications with a phone on the LAN
Exec=kdeconnect-app
Icon=kdeconnect
Terminal=false
StartupWMClass=org.kde.kdeconnect.app
Categories=Qt;KDE;Network;
Keywords=phone;android;sync;share;clipboard;notification;kdeconnect;
DESKTOP
cat > "$_apps/org.kde.kdeconnect.sms.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KDE Connect SMS
GenericName=Text Messages
Comment=Read and send a paired phone's text messages
Exec=kdeconnect-sms
Icon=kdeconnect
Terminal=false
StartupWMClass=org.kde.kdeconnect.sms
Categories=Qt;KDE;Network;
Keywords=sms;text;message;phone;kdeconnect;
DESKTOP
cat > "$_apps/org.kde.kdeconnect.nonplasma.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KDE Connect Indicator
Comment=Paired phones in the panel tray
Exec=kdeconnect-indicator
Icon=kdeconnect
Terminal=false
StartupWMClass=org.kde.kdeconnect-indicator
Categories=Qt;KDE;Network;
Keywords=phone;tray;kdeconnect;
DESKTOP
chmod 644 "$_apps/org.kde.kdeconnect.app.desktop" \
	"$_apps/org.kde.kdeconnect.sms.desktop" \
	"$_apps/org.kde.kdeconnect.nonplasma.desktop"
