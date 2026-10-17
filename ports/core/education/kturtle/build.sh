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

cat > "$PKG/usr/share/applications/org.kde.kturtle.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KTurtle
GenericName=Educational Programming Environment
Comment=Learn to program by steering a turtle
TryExec=kturtle
Exec=kturtle
Icon=kturtle
Terminal=false
StartupWMClass=org.kde.kturtle
X-DocPath=kturtle/index.html
Categories=Qt;KDE;Education;ComputerScience;
Keywords=programming;logo;turtle;learn;code;children;kturtle;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kturtle.desktop"
