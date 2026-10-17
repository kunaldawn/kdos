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

patch -p1 -i "$PORT_SRC/kde-key.patch"
cmake -B build -G Ninja -Wno-dev \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DDISABLE_DBUS=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

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
