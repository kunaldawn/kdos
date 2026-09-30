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

# BUILD_WITH_QT6 selects Qt 6 and KF6; without it the project looks for Qt 5.
# The project forces its own libraries static, so BUILD_SHARED_LIBS is not
# passed. The system potrace is used rather than the copy in external/.
# Python scripting and its plugins build against python3 and the pybind11
# bundled under external/QtAppSetup; nothing is fetched. KF6BreezeIcons is an
# optional find that compiles the Breeze icon set into the program: required
# here, so the toolbar is not left blank under an icon theme that lacks a name.
# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only. The Google Fonts, LottieFiles and emoji-download dialogs reach the
# network when opened and report the failure offline; there is no switch.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
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

# Upstream installs the SVG logo under 512x512/apps with a .png name, and the
# panel draws application icons from real hicolor PNGs only.
for s in 48 64 128 256 512; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s -o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.kde.glaxnimate.png" \
		data/logo/logo.svg
done

# KAboutData::setDesktopFileName makes the Wayland app_id org.kde.glaxnimate.
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
