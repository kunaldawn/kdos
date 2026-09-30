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

# Each optional library upstream drops silently when absent is required here:
# libkdcraw is RAW decoding, CFitsio the FITS loader, TIFF the libtiff log
# filter, and Baloo the semantic backend that keeps ratings and tags. kImage-
# Annotator is the annotate tool and is already required by its own option.
# Purpose is the Share menu, whose plugins are online services, and Plasma
# Activities belongs to the Plasma shell; both are kept out. The X11 half is
# what Gwenview uses under Xwayland. KF_SKIP_PO_PROCESSING leaves the
# interface catalogues out: bundled data is English only. kdoctools_install()
# builds every translated handbook too, and those are removed after the
# install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D GWENVIEW_SEMANTICINFO_BACKEND=Baloo \
	-D GWENVIEW_IMAGEANNOTATOR=ON \
	-D WITHOUT_X11=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KDcrawQt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_CFitsio=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_TIFF=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KF6Purpose=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_PlasmaActivities=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED. KAboutData derives the desktop file name, and
# so the Wayland app_id, as org.kde.gwenview, which StartupWMClass has to
# name. inode/directory is left out of MimeType: claimed here, a folder could
# open in Gwenview instead of the file manager. The hicolor PNGs it names are
# upstream's, installed above.
cat > "$PKG/usr/share/applications/org.kde.gwenview.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Gwenview
GenericName=Image Viewer
Comment=Browse and view images
TryExec=gwenview
Exec=gwenview %U
Icon=gwenview
Terminal=false
StartupWMClass=org.kde.gwenview
X-DocPath=gwenview/index.html
X-DBUS-ServiceName=org.kde.gwenview
MimeType=image/avif;image/gif;image/heif;image/jpeg;image/jxl;image/png;image/bmp;image/x-eps;image/x-icns;image/x-ico;image/x-portable-bitmap;image/x-portable-graymap;image/x-portable-pixmap;image/x-xbitmap;image/x-xpixmap;image/tiff;image/x-psd;image/x-webp;image/webp;image/x-tga;image/x-xcf;image/openraster;image/svg+xml;image/svg+xml-compressed;application/x-krita;image/x-kde-raw;image/x-canon-cr2;image/x-canon-crw;image/x-kodak-dcr;image/x-adobe-dng;image/x-kodak-k25;image/x-kodak-kdc;image/x-minolta-mrw;image/x-nikon-nef;image/x-olympus-orf;image/x-pentax-pef;image/x-fuji-raf;image/x-panasonic-rw;image/x-sony-sr2;image/x-sony-srf;image/x-sigma-x3f;image/x-sony-arw;image/x-panasonic-rw2;
Categories=Qt;KDE;Graphics;Viewer;Photography;
Keywords=image;picture;photo;viewer;browser;slideshow;raw;gwenview;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.gwenview.desktop"
