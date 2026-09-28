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

# WITHOUT_X11 drops the keyboard layout detection, which reads the X server's
# XKB state and so sees nothing under the Wayland platform this session runs
# Qt applications on; the layout is chosen in the program instead. The
# handbook is optional upstream and dropped silently without kdoctools, so it
# is required here. The statistics charts are the org.kde.charts QML module
# from kqtquickcharts, loaded at run time. KF_SKIP_PO_PROCESSING leaves the
# translation catalogues out: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D WITHOUT_X11=ON \
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
# app_id org.kde.ktouch.
cat > "$PKG/usr/share/applications/org.kde.ktouch.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KTouch
GenericName=Touch Typing Tutor
Comment=Learn to type without looking at the keys
TryExec=ktouch
Exec=ktouch
Icon=ktouch
Terminal=false
StartupWMClass=org.kde.ktouch
X-DocPath=ktouch/index.html
Categories=Qt;KDE;Education;
Keywords=typing;keyboard;tutor;touch;lesson;ktouch;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.ktouch.desktop"
