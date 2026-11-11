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

desktop-file-edit --set-name="Terminal (QMLKonsole)" "$PKG/usr/share/applications/org.kde.qmlkonsole.desktop"

install -d "$PKG/usr/share/kdos/xdg"
cat > "$PKG/usr/share/kdos/xdg/qmlkonsolerc" <<'EOF'
[General]
colorScheme=KDOS-Bone
EOF
chmod 644 "$PKG/usr/share/kdos/xdg/qmlkonsolerc"
