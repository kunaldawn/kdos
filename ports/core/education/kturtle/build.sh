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

# The handbook is optional upstream and dropped silently without kdoctools, so
# it is required here. KF_SKIP_PO_PROCESSING leaves the translation catalogues
# out: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
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
# app_id org.kde.kturtle.
cat > "$PKG/usr/share/applications/org.kde.kturtle.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KTurtle
GenericName=Educational Programming Environment
Comment=Learn to program by steering a turtle
TryExec=kturtle
Exec=kturtle
Icon=kturtle
Terminal=false
StartupWMClass=org.kde.kturtle
X-DocPath=kturtle/index.html
Categories=Qt;KDE;Education;ComputerScience;
Keywords=programming;logo;turtle;learn;code;children;kturtle;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kturtle.desktop"
