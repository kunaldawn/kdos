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

# 3.13 is the current release and is Qt 5 only. QScintilla, QCustomPlot and
# QHexEdit are the copies under libs/: the qscintilla port is the Qt 6 build,
# which a Qt 5 program cannot link. BUILD_STABLE_VERSION keeps the build date
# out of the version string, which would otherwise differ on every build.
# SQLCipher is off: plain SQLite from the sqlite port. The version check is
# compiled in only on Windows and macOS, so this build never contacts the
# project's server. Translations are compiled into the program as resources
# and have no switch.
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

# The project file type upstream defines is not installed by its CMake.
install -Dm644 distri/mime/packages/db4s-sqbpro.xml \
	"$PKG/usr/share/mime/packages/db4s-sqbpro.xml"

# UPSTREAM'S ENTRY IS REPLACED: of the types it claims, text/csv belongs to
# the spreadsheets and three are defined nowhere; it keeps the project type
# and the two SQLite database types. The Qt 5 Wayland app_id and the X11
# class are both the executable's name, sqlitebrowser.
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
