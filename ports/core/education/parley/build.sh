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

# BUILD_BROWSERINTEGRATION is the web lookup of translations and pictures,
# which only works online, so it is off and QtWebEngine is not linked. The
# HTML export needs libxslt and libxml2, is dropped silently without them, and
# is required here. KF_SKIP_PO_PROCESSING leaves the translation catalogues
# out: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
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

# kdoctools_install() builds every translated handbook and manual page; only
# the English ones are kept.
for d in "$PKG"/usr/share/doc/HTML/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/HTML/en | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

# The entry is replaced for StartupWMClass: KAboutData makes the Wayland
# app_id org.kde.parley. KWordQuiz is the entry that claims KVTML files.
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
