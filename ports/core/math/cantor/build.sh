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
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_DOC=ON \
	-D ENABLE_EMBEDDED_DOCUMENTATION=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Poppler=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Analitza6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_R=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qalculate=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Python3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LuaJIT=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Julia=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/cantor"

cat > "$PKG/usr/share/applications/org.kde.cantor.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Cantor
GenericName=Mathematical Worksheet
Comment=Worksheets for Python, R, Octave, Maxima, Qalculate and KAlgebra
TryExec=cantor
Exec=cantor %U
Icon=cantor
Terminal=false
StartupWMClass=org.kde.cantor
X-DocPath=cantor/index.html
MimeType=application/x-cantor;
Categories=Qt;KDE;Education;Math;Science;
Keywords=math;worksheet;notebook;python;r;octave;maxima;calculator;cantor;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.cantor.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/cantor.png
test -e "$PKG"/usr/share/mime/packages/cantor.xml
