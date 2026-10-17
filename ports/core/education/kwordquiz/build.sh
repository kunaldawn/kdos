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

install -Dm644 /dev/stdin "$PKG/usr/share/mime/packages/kdos-kvtml.xml" <<'MIMEXML'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-kvtml">
    <comment>KDE vocabulary document</comment>
    <sub-class-of type="application/xml"/>
    <glob pattern="*.kvtml"/>
  </mime-type>
</mime-info>
MIMEXML
cat > "$PKG/usr/share/applications/org.kde.kwordquiz.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KWordQuiz
GenericName=Flash Card Trainer
Comment=Learn vocabulary with flashcards and quizzes
TryExec=kwordquiz
Exec=kwordquiz %u
Icon=kwordquiz
Terminal=false
StartupWMClass=org.kde.kwordquiz
X-DocPath=kwordquiz/index.html
MimeType=application/x-kvtml;
Categories=Qt;KDE;Education;Languages;
Keywords=flashcard;vocabulary;quiz;learn;language;kvtml;kwordquiz;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kwordquiz.desktop"
