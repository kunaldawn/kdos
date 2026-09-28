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


# kde-key.patch makes the plugin answer QT_QPA_PLATFORMTHEME=kde as well as
# =qt5ct and =qt6ct. The session exports kde, and no other Qt 5 platform theme
# answers it, so this is how every Qt 5 program reads the qt5ct files `kdos
# theme` writes. Qt 5 searches only its own platformthemes directory, so the
# key cannot shadow plasma-integration's Qt 6 plugin. qt5-qtwayland gives the
# settings window its Wayland platform plugin. D-Bus
# stays on: it carries the portal file dialogs and the StatusNotifierItem tray
# icon. Translations are compiled into the program by lrelease, which is why
# qt5-qttools is a build dependency.
patch -p1 -i "$PORT_SRC/kde-key.patch"
cmake -B build -G Ninja -Wno-dev \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DDISABLE_DBUS=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

# UPSTREAM'S ENTRY IS REPLACED to add StartupWMClass and an icon the menu can
# draw. setDesktopFileName("qt5ct.desktop") makes the Wayland app_id qt5ct.
# The icon is Breeze's preferences-desktop-theme rasterised into hicolor:
# upstream names that theme icon, and the panel reads only PNG.
for _size in 32 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	rsvg-convert -w "$_size" -h "$_size" \
		-o "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/qt5ct.png" \
		/usr/share/icons/breeze/preferences/32/preferences-desktop-theme.svg
done
cat > "$PKG/usr/share/applications/qt5ct.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Qt 5 Settings
GenericName=Qt 5 Appearance
Comment=Palette, style, fonts and icons of Qt 5 programs
Exec=qt5ct
Icon=qt5ct
Terminal=false
StartupWMClass=qt5ct
Categories=Settings;DesktopSettings;Qt;
Keywords=settings;desktop;qt;qt5;theme;style;palette;font;icons;
DESKTOP
chmod 644 "$PKG/usr/share/applications/qt5ct.desktop"
