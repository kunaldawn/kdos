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

patch -p1 -i "$PORT_SRC/ai-tools-default-off.patch"
patch -p1 -i "$PORT_SRC/no-online-export.patch"

# BUILD_WITH_QT6 selects Qt 6 and KF6; without it the project looks for Qt 5.
#
# The collection lives in SQLite. MySQL support needs Qt's MySQL driver, which
# qt6-qtbase does not build, and the internal server needs a mysqld; both off.
# KFileMetaData and Akonadi contacts are Plasma's indexer and address book,
# neither of which runs here. Jasper is not a port: without it the bundled
# LibRaw has no RedCine decoder and nothing else changes.
#
# Geolocation uses the bundled map engine, whose Blue Marble and City Lights
# maps are in the tarball and draw offline; the OpenStreetMap tiles need the
# network. The media player is Qt Multimedia over FFmpeg.
#
# Each library the rest of the feature set rests on is an optional find, and a
# missing one builds a digiKam without that tool; each is required here:
#   X11 (with Xv)      the monitor colour profile under Xwayland
#   KF6 KIO, IconThemes, Notifications, NotifyConfig, Sonnet
#                      desktop integration, notifications, spell-checking
#   KF6ThreadWeaver    the panorama tool
#   KF6CalendarCore    the calendar tool
#   Qt6Multimedia      the video player
#   LensFun            the lens auto-correction tool
#   Gphoto2            camera import over USB
#   Eigen3             the refocus tool
#   ImageMagick        the extra codecs
#   GLIB2              the content-aware resizer (liblqr is bundled)
#   Libheif, X265      HEIF reading and writing
#   Libjxl             JPEG XL, and DNG writing from it
#   FLEX, BISON        the panorama tool
#   LibXml2, LibXslt   the HTML gallery
#   KSaneWidgets6      the flatbed scanner import
#
# DIGIKAMSC_COMPILE_PO=OFF and KF_SKIP_PO_PROCESSING leave every translation
# out: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_WITH_QT6=ON \
	-D DIGIKAMSC_COMPILE_PO=OFF \
	-D DIGIKAMSC_COMPILE_DIGIKAM=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D ENABLE_MYSQLSUPPORT=OFF \
	-D ENABLE_INTERNALMYSQL=OFF \
	-D ENABLE_KFILEMETADATASUPPORT=OFF \
	-D ENABLE_AKONADICONTACTSUPPORT=OFF \
	-D ENABLE_GEOLOCATION=ON \
	-D ENABLE_MEDIAPLAYER=ON \
	-D ENABLE_DBUS=ON \
	-D ENABLE_KIO=ON \
	-D ENABLE_APPSTYLES=OFF \
	-D ENABLE_SHOWFOTO=ON \
	-D ENABLE_DIGIKAM_MODELTEST=OFF \
	-D ENABLE_SANITIZERS=OFF \
	-D BUILD_WITH_CCACHE=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_Jasper=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LensFun=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Gphoto2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Eigen3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_ImageMagick=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_GLIB2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Libheif=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_X265=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Libjxl=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_FLEX=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_BISON=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibXslt=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KSaneWidgets6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6KIO=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6IconThemes=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6ThreadWeaver=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6NotifyConfig=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Notifications=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Sonnet=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6CalendarCore=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Multimedia=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6MultimediaWidgets=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The face engine looks for its models in digikam/facesengine under the XDG
# data directories before it offers to download them, so face detection and
# recognition work on first start with no network. File names and sizes are
# the ones dnnmodels.conf lists; a size that differs counts as missing.
install -d "$PKG/usr/share/digikam/facesengine"
install -m644 face_detection_yunet_2023mar.onnx face_recognition_sface_2021dec.onnx \
	dnntestimage.jpeg "$PKG/usr/share/digikam/facesengine/"

# KAboutData makes the Wayland app_ids org.kde.digikam and org.kde.showfoto.
# No MimeType on showFoto: the image types already have a handler here, and a
# second claim would make the default whichever entry sorts first.
cat > "$PKG/usr/share/applications/org.kde.digikam.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=digiKam
GenericName=Photo Library
Comment=Organise, tag, search and edit a photo collection
Exec=digikam
Icon=digikam
Terminal=false
StartupWMClass=org.kde.digikam
Categories=Qt;KDE;Graphics;Photography;
Keywords=photo;library;album;tag;face;raw;camera;import;digikam;
EOF
cat > "$PKG/usr/share/applications/org.kde.showfoto.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=showFoto
GenericName=Photo Editor
Comment=View and edit single photos with digiKam's tools
Exec=showfoto %U
Icon=showfoto
Terminal=false
StartupWMClass=org.kde.showfoto
Categories=Qt;KDE;Graphics;Photography;
Keywords=photo;editor;viewer;raw;showfoto;
EOF
chmod 644 "$PKG/usr/share/applications/org.kde.digikam.desktop" \
	"$PKG/usr/share/applications/org.kde.showfoto.desktop"
