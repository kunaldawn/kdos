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
	-D ENABLE_AUTO=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/kdiff3"

cat > "$PKG/usr/share/applications/org.kde.kdiff3.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KDiff3
GenericName=Diff and Merge Tool
Comment=Compare and merge two or three files or folders
TryExec=kdiff3
Exec=kdiff3 %U
Icon=kdiff3
Terminal=false
StartupWMClass=org.kde.kdiff3
X-DocPath=kdiff3/index.html
Categories=Qt;KDE;Development;RevisionControl;
Keywords=diff;merge;compare;patch;three-way;folder;kdiff3;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kdiff3.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/kdiff3.png
