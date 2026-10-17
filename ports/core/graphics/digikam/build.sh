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

cmake -S . -B build -G Ninja \
	-D CMAKE_C_STANDARD=99 \
	-D CMAKE_C_EXTENSIONS=ON \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
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
	-D CMAKE_REQUIRE_FIND_PACKAGE_Jasper=ON \
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

install -d "$PKG/usr/share/digikam/facesengine"
install -m644 face_detection_yunet_2023mar.onnx face_recognition_sface_2021dec.onnx \
	dnntestimage.jpeg "$PKG/usr/share/digikam/facesengine/"

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
