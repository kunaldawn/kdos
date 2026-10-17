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
	-D WITHOUT_X11=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.ktouch.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KTouch
GenericName=Touch Typing Tutor
Comment=Learn to type without looking at the keys
TryExec=ktouch
Exec=ktouch
Icon=ktouch
Terminal=false
StartupWMClass=org.kde.ktouch
X-DocPath=ktouch/index.html
Categories=Qt;KDE;Education;
Keywords=typing;keyboard;tutor;touch;lesson;ktouch;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.ktouch.desktop"
