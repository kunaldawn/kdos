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

# WEBVIEW_PROVIDER=none: the Syncthing web UI opens in the browser, so no
# QtWebEngine is linked. NO_PLASMOID and NO_FILE_ITEM_ACTION_PLUGIN: both
# need Plasma or Dolphin's KIO plugin interface and nothing here loads them.
# USE_BOOST_PROCESS=OFF: the launcher runs Syncthing through QProcess; the
# Boost.Process it would use otherwise is the deprecated v1 interface.
# SYSTEMD_SUPPORT=OFF: there is no systemd user unit to control.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D BUILD_TESTING=OFF \
	-D QT_PACKAGE_PREFIX=Qt6 \
	-D KF_PACKAGE_PREFIX=KF6 \
	-D WEBVIEW_PROVIDER=none \
	-D JS_PROVIDER=qml \
	-D WIDGETS_GUI=ON \
	-D QUICK_GUI=OFF \
	-D NO_LIBSYNCTHING=ON \
	-D NO_PLASMOID=ON \
	-D NO_FILE_ITEM_ACTION_PLUGIN=ON \
	-D SETUP_TOOLS=OFF \
	-D SYSTEMD_SUPPORT=OFF \
	-D USE_BOOST_PROCESS=OFF \
	-D DBUS_BASED_POWER_MONITORING=ON \
	-D NO_DOXYGEN=ON \
	-D NO_SPHINX=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share" -name '*.qm' ! -name '*_en*.qm' -delete

# The panel draws only PNG icons, and upstream installs the logo as SVG.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s tray/resources/icons/hicolor/scalable/apps/syncthingtray.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/syncthingtray.png"
done
install -Dm644 tray/resources/icons/hicolor/256x256/apps/syncthingtray.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/syncthingtray.png"

# The window's app_id is the program's name, which StartupWMClass must repeat;
# upstream's entry carries no StartupWMClass.
cat > "$PKG/usr/share/applications/syncthingtray.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Syncthing Tray
GenericName=Syncthing Status
Comment=Status and control of the local Syncthing from the panel tray
Exec=syncthingtray
Icon=syncthingtray
Terminal=false
StartupWMClass=syncthingtray
Categories=Network;FileTransfer;
Keywords=syncthing;sync;folder;device;share;
Actions=ShowSyncthing;RescanAll;

[Desktop Action ShowSyncthing]
Name=Open the Syncthing web UI
Exec=syncthingtray qt-widgets-gui --webui

[Desktop Action RescanAll]
Name=Rescan all folders
Exec=syncthingctl rescan-all
DESKTOP
chmod 644 "$PKG/usr/share/applications/syncthingtray.desktop"
