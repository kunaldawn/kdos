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

# KSaneWidgets6 is the Acquire Image scanner import, optional upstream and
# silently dropped when absent, so it is required here. KF_SKIP_PO_PROCESSING
# leaves the interface catalogues out: bundled data is English only. The
# handbook is built, so Help opens a local copy; kdoctools_install() builds
# every translated handbook too, and those are removed after the install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_DOC=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KSaneWidgets6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData sets the desktop
# file name, so the Wayland app_id is org.kde.kolourpaint, not upstream's
# "kolourpaint". The hicolor PNGs it names are upstream's, installed above.
cat > "$PKG/usr/share/applications/org.kde.kolourpaint.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KolourPaint
GenericName=Paint Program
Comment=An easy-to-use paint program
TryExec=kolourpaint
Exec=kolourpaint %u
Icon=kolourpaint
Terminal=false
StartupWMClass=org.kde.kolourpaint
X-DocPath=kolourpaint/index.html
MimeType=application/x-krita;application/x-navi-animation;image/avif;image/bmp;image/gif;image/heif;image/jpeg;image/jxl;image/openraster;image/png;image/svg+xml;image/svg+xml-compressed;image/tiff;image/vnd.adobe.photoshop;image/vnd.microsoft.icon;image/vnd.wap.wbmp;image/webp;image/x-eps;image/x-exr;image/x-hdr;image/x-icns;image/x-mng;image/x-pcx;image/x-pic;image/x-portable-bitmap;image/x-portable-graymap;image/x-portable-pixmap;image/x-rgb;image/x-sun-raster;image/x-tga;image/x-xbitmap;image/x-xcf;image/x-xpixmap;
Categories=Qt;KDE;Graphics;2DGraphics;RasterGraphics;
Keywords=paint;draw;drawing;crop;image;picture;kolourpaint;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kolourpaint.desktop"
