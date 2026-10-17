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
	-D USE_UNITY_CMAKE_SUPPORT=OFF \
	-D USE_DBUS=ON \
	-D DISABLE_KWATCHGNUPG=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KPim6IdentityManagementCore=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KPim6MailTransport=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KPim6AkonadiMime=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.kleopatra.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Kleopatra
GenericName=Certificate Manager
Comment=OpenPGP and S/MIME keys, signing and encryption with GnuPG
Exec=kleopatra
Icon=kleopatra
Terminal=false
StartupNotify=true
StartupWMClass=org.kde.kleopatra
Categories=Qt;KDE;Utility;Security;
Keywords=gpg;gnupg;pgp;openpgp;smime;key;certificate;encrypt;decrypt;sign;verify;
X-DocPath=kleopatra/index.html
EOF2
cat > "$PKG/usr/share/applications/org.kde.kwatchgnupg.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=GnuPG Log Viewer
Comment=Viewer for GnuPG daemon and application logs
Exec=kwatchgnupg
Icon=org.kde.kwatchgnupg
Terminal=false
StartupWMClass=org.kde.kwatchgnupg
SingleMainWindow=true
Categories=Qt;KDE;System;Security;
Keywords=gpg;gnupg;log;watch;kwatchgnupg;
EOF2
chmod 644 "$PKG"/usr/share/applications/org.kde.kleopatra.desktop \
	"$PKG"/usr/share/applications/org.kde.kwatchgnupg.desktop
