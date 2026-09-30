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

# KF_SKIP_PO_PROCESSING leaves the translation catalogues out: bundled data is
# English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
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
# app_id org.kde.kgeography.
cat > "$PKG/usr/share/applications/org.kde.kgeography.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KGeography
GenericName=Geography Trainer
Comment=Learn maps, capitals and flags
TryExec=kgeography
Exec=kgeography
Icon=kgeography
Terminal=false
StartupWMClass=org.kde.kgeography
X-DocPath=kgeography/index.html
Categories=Qt;KDE;Education;Geography;
Keywords=geography;map;country;capital;flag;quiz;kgeography;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kgeography.desktop"
