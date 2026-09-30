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

# WITH_WEBENGINE=OFF: link previews fetch every URL posted in a channel, and
# Qt 5's WebEngine is not ported. The core's Blowfish channel encryption needs
# qca-qt5 and its ossl provider at run time; it is required here so a build
# that misses it fails instead of dropping the commands.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D USE_CCACHE=OFF \
	-D WANT_CORE=ON \
	-D WANT_QTCLIENT=ON \
	-D WANT_MONO=ON \
	-D WITH_KDE=OFF \
	-D WITH_BUNDLED_ICONS=ON \
	-D WITH_OXYGEN_ICONS=OFF \
	-D WITH_WEBKIT=OFF \
	-D WITH_WEBENGINE=OFF \
	-D WITH_LDAP=ON \
	-D EMBED_DATA=OFF \
	-D BUILD_TESTING=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_LibsnoreQt5=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qca-qt5=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt5Multimedia=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt5DBus=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Ldap=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/quassel/translations" -name '*.qm' ! -name '*_en*.qm' -delete

# Each binary sets its desktop file name, and so its app_id, to its own
# name, which StartupWMClass must repeat.
_apps="$PKG/usr/share/applications"
cat > "$_apps/quassel.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Quassel IRC
GenericName=IRC Client
Comment=IRC client with its own core, in one program
Exec=quassel
Icon=quassel
Terminal=false
StartupWMClass=quassel
Categories=Qt;Network;Chat;IRCClient;
Keywords=irc;chat;channel;quassel;
DESKTOP
cat > "$_apps/quasselclient.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Quassel IRC (Client only)
GenericName=IRC Client
Comment=Connect to a Quassel core that stays online
Exec=quasselclient
Icon=quassel
Terminal=false
StartupWMClass=quasselclient
Categories=Qt;Network;Chat;IRCClient;
Keywords=irc;chat;channel;quassel;core;
DESKTOP
chmod 644 "$_apps/quassel.desktop" "$_apps/quasselclient.desktop"
