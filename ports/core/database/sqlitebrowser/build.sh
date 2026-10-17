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
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_STABLE_VERSION=ON \
	-DENABLE_TESTING=OFF \
	-DFORCE_INTERNAL_QSCINTILLA=ON \
	-DFORCE_INTERNAL_QCUSTOMPLOT=ON \
	-DFORCE_INTERNAL_QHEXEDIT=ON \
	-Dsqlcipher=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/sqlitebrowser"

install -Dm644 distri/mime/packages/db4s-sqbpro.xml \
	"$PKG/usr/share/mime/packages/db4s-sqbpro.xml"

cat > "$PKG/usr/share/applications/sqlitebrowser.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=DB Browser for SQLite
GenericName=Database Browser
Comment=Create, browse, edit and query SQLite databases
TryExec=sqlitebrowser
Exec=sqlitebrowser %f
Icon=sqlitebrowser
Terminal=false
StartupWMClass=sqlitebrowser
Categories=Development;Utility;Database;
MimeType=application/vnd.db4s-project+xml;application/vnd.sqlite3;application/x-sqlite2;
Keywords=sqlite;database;sql;table;query;browser;db4s;
DESKTOP
chmod 644 "$PKG/usr/share/applications/sqlitebrowser.desktop"
test -e "$PKG"/usr/share/icons/hicolor/256x256/apps/sqlitebrowser.png
