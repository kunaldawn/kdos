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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s -o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/org.kde.skanlite.png" \
		sc-apps-org.kde.skanlite.svg
done

cat > "$PKG/usr/share/applications/org.kde.skanlite.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Skanlite
GenericName=Image Scanning Application
Comment=Scan and save images
Exec=skanlite
Icon=org.kde.skanlite
Terminal=false
StartupWMClass=org.kde.skanlite
X-DocPath=skanlite/index.html
Categories=Qt;KDE;Graphics;Scanning;
Keywords=scan;scanner;sane;image;photo;document;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.skanlite.desktop"
