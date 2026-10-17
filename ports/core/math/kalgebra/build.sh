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
	-D CMAKE_DISABLE_FIND_PACKAGE_Plasma=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Readline=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Curses=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/kalgebra"
test -x "$PKG/usr/bin/calgebra"

cat > "$PKG/usr/share/applications/org.kde.kalgebra.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KAlgebra
GenericName=Graph Calculator
Comment=Math expression solver and plotter
TryExec=kalgebra
Exec=kalgebra
Icon=kalgebra
Terminal=false
StartupWMClass=org.kde.kalgebra
X-DocPath=kalgebra/index.html
Categories=Qt;KDE;Education;Math;Science;
Keywords=calculator;graph;plot;math;algebra;function;kalgebra;
DESKTOP
cat > "$PKG/usr/share/applications/org.kde.kalgebramobile.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KAlgebra (touch)
GenericName=Graph Calculator
Comment=Math expression solver and plotter for touch screens
TryExec=kalgebramobile
Exec=kalgebramobile
Icon=kalgebra
Terminal=false
StartupWMClass=org.kde.kalgebramobile
Categories=Qt;KDE;Education;Math;Science;
Keywords=calculator;graph;plot;math;algebra;touch;kalgebra;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/org.kde.kalgebra*.desktop
