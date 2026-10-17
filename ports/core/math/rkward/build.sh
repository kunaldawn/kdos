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
	-D FORCE_WITH_QWEBENGINE=ON \
	-D FORCE_WITH_QWEBVIEW=OFF \
	-D DLOPEN_RLIB=ON \
	-D R_EXECUTABLE=/usr/bin/R \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KDSingleApplication-qt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/rkward"
test -e "$PKG"/usr/share/rkward/rpackages/rkward.tgz

cat > "$PKG/usr/share/applications/org.kde.rkward.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=RKWard
GenericName=Statistics with R
Comment=Data analysis, plots and reports with the R language
TryExec=rkward
Exec=rkward --autoreuse %F
Icon=rkward
Terminal=false
StartupWMClass=org.kde.rkward
X-DocPath=rkward/index.html
Categories=Qt;KDE;Science;Math;NumericalAnalysis;
MimeType=text/r;application/rdata;text/vnd.kde.rmarkdown;application/vnd.kde.rkward-output;
Keywords=statistics;data;r;regression;analysis;plot;rkward;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.rkward.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/rkward.png
