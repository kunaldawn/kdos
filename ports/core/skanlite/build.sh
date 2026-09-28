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

# KF_SKIP_PO_PROCESSING leaves the interface catalogues out: bundled data is
# English only. The handbook is built, so Help opens a local copy;
# kdoctools_install() builds every translated handbook too, and those are
# removed after the install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# Upstream ships its icon as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s -o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/org.kde.skanlite.png" \
		sc-apps-org.kde.skanlite.svg
done

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData sets the desktop
# file name, so the Wayland app_id is org.kde.skanlite, not upstream's
# "skanlite".
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
