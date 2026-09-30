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
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_DOC=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qca-qt6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# KAboutData names the window's app_id org.kde.konversation, which
# StartupWMClass must repeat; upstream's entry carries the X11 class.
cat > "$PKG/usr/share/applications/org.kde.konversation.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Konversation
GenericName=IRC Client
Comment=Chat on IRC networks and channels
Exec=konversation -qwindowtitle %c %u
Icon=konversation
Terminal=false
StartupWMClass=org.kde.konversation
SingleMainWindow=true
MimeType=x-scheme-handler/irc;x-scheme-handler/ircs;
Categories=Qt;KDE;Network;IRCClient;
Keywords=irc;chat;channel;dcc;konversation;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.konversation.desktop"
