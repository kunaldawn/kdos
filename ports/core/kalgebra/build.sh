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

# The desktop window is built only when DocTools, WebEngineWidgets,
# ConfigWidgets, WidgetsAddons and KIO are all found, and is dropped without a
# word otherwise; the test after the install turns that into a failure.
# Readline and ncurses give calgebra, the console calculator, which is dropped
# the same way. There is no Plasma, so the plasmoid is not built.
# KF_SKIP_PO_PROCESSING leaves the translation catalogues out: bundled data is
# English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Plasma=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Readline=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Curses=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/kalgebra"
test -x "$PKG/usr/bin/calgebra"

# kdoctools_install() builds every translated handbook and manual page; only
# the English ones are kept.
for d in "$PKG"/usr/share/doc/HTML/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/HTML/en | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

# The entries are replaced for StartupWMClass: KAboutData makes the Wayland
# app_id org.kde.<program>. application/x-kalgebra is not in the MIME
# database, so neither entry claims it. KAlgebra Mobile is the touch
# interface, a Kirigami window.
cat > "$PKG/usr/share/applications/org.kde.kalgebra.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KAlgebra
GenericName=Graph Calculator
Comment=Math expression solver and plotter
TryExec=kalgebra
Exec=kalgebra
Icon=kalgebra
Terminal=false
StartupWMClass=org.kde.kalgebra
X-DocPath=kalgebra/index.html
Categories=Qt;KDE;Education;Math;Science;
Keywords=calculator;graph;plot;math;algebra;function;kalgebra;
DESKTOP
cat > "$PKG/usr/share/applications/org.kde.kalgebramobile.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KAlgebra (touch)
GenericName=Graph Calculator
Comment=Math expression solver and plotter for touch screens
TryExec=kalgebramobile
Exec=kalgebramobile
Icon=kalgebra
Terminal=false
StartupWMClass=org.kde.kalgebramobile
Categories=Qt;KDE;Education;Math;Science;
Keywords=calculator;graph;plot;math;algebra;touch;kalgebra;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/org.kde.kalgebra*.desktop
