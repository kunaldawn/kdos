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
	-D BUILD_WITH_QT6=ON \
	-D GLAXNIMATE_SYSTEM_POTRACE=ON \
	-D MOBILE_UI=OFF \
	-D VERSION_SUFFIX= \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6BreezeIcons=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

for s in 48 64 128 256 512; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s -o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.kde.glaxnimate.png" \
		data/logo/logo.svg
done

cat > "$PKG/usr/share/applications/org.kde.glaxnimate.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Glaxnimate
GenericName=Vector Animation
Comment=Create vector animations and export them as Lottie, SVG, GIF or video
Exec=glaxnimate %F
Icon=org.kde.glaxnimate
Terminal=false
StartupWMClass=org.kde.glaxnimate
Categories=Qt;KDE;Graphics;VectorGraphics;
Keywords=animation;vector;lottie;motion;svg;glaxnimate;
EOF
chmod 644 "$PKG/usr/share/applications/org.kde.glaxnimate.desktop"
