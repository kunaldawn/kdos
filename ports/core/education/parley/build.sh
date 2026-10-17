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
	-D BUILD_BROWSERINTEGRATION=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibXslt=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.parley.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Parley
GenericName=Vocabulary Trainer
Comment=Memorise vocabulary with spaced repetition
TryExec=parley
Exec=parley
Icon=parley
Terminal=false
StartupWMClass=org.kde.parley
X-DocPath=parley/index.html
Categories=Qt;KDE;Education;Languages;
Keywords=vocabulary;flashcard;language;learn;memorize;kvtml;parley;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.parley.desktop"
