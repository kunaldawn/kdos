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


# The plugin answers QT_QPA_PLATFORMTHEME=qt6ct and =qt5ct, and qt5ct's
# answers both too, so one value themes Qt 6 and Qt 5 programs alike.
# LinguistTools is kept out of the search, so the settings program is built
# English only and without the translations the tarball carries.
cmake -B build -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt6LinguistTools=ON
ninja -C build
DESTDIR=$PKG ninja -C build install

# UPSTREAM'S ENTRY IS REPLACED to add StartupWMClass and an icon the menu can
# draw. setDesktopFileName("qt6ct") makes the Wayland app_id qt6ct. The icon
# is Breeze's preferences-desktop-theme rasterised into hicolor: upstream
# names that theme icon, and the panel reads only PNG.
for _size in 32 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	rsvg-convert -w "$_size" -h "$_size" \
		-o "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/qt6ct.png" \
		/usr/share/icons/breeze/preferences/32/preferences-desktop-theme.svg
done
cat > "$PKG/usr/share/applications/qt6ct.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Qt 6 Settings
GenericName=Qt 6 Appearance
Comment=Palette, style, fonts and icons of Qt 6 programs
Exec=qt6ct
Icon=qt6ct
Terminal=false
StartupWMClass=qt6ct
Categories=Settings;DesktopSettings;Qt;
Keywords=settings;desktop;qt;qt6;theme;style;palette;font;icons;
DESKTOP
chmod 644 "$PKG/usr/share/applications/qt6ct.desktop"
