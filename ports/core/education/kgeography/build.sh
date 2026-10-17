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
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.kgeography.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KGeography
GenericName=Geography Trainer
Comment=Learn maps, capitals and flags
TryExec=kgeography
Exec=kgeography
Icon=kgeography
Terminal=false
StartupWMClass=org.kde.kgeography
X-DocPath=kgeography/index.html
Categories=Qt;KDE;Education;Geography;
Keywords=geography;map;country;capital;flag;quiz;kgeography;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kgeography.desktop"
