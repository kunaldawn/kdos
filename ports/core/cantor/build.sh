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

# Backends built: Python, R (through libR.so), Qalculate, KAlgebra (Analitza)
# and Lua (LuaJIT). Octave, Maxima, Sage and Scilab are run as programs, so
# their backends always build and work when the program is installed. Julia
# is not a port and is off. Poppler's Qt 6 binding is linked by cantorlibs
# unconditionally. The patched Discount in thirdparty/ is compiled from the
# tarball, not fetched. KF_SKIP_PO_PROCESSING leaves the translation
# catalogues out: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_DOC=ON \
	-D ENABLE_EMBEDDED_DOCUMENTATION=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Poppler=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Analitza6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_R=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qalculate=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Python3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LuaJIT=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Julia=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/cantor"

# kdoctools_install() builds every translated handbook and manual page; only
# the English ones are kept.
for d in "$PKG"/usr/share/doc/HTML/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/HTML/en | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

# UPSTREAM'S ENTRY IS REPLACED: KAboutData makes the Wayland app_id
# org.kde.cantor, and the -qwindowicon and -qwindowtitle arguments upstream
# passes are X11 conventions the entry does not need. application/x-cantor is
# defined by the cantor.xml the port installs; the PNG icons are named cantor.
cat > "$PKG/usr/share/applications/org.kde.cantor.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Cantor
GenericName=Mathematical Worksheet
Comment=Worksheets for Python, R, Octave, Maxima, Qalculate and KAlgebra
TryExec=cantor
Exec=cantor %U
Icon=cantor
Terminal=false
StartupWMClass=org.kde.cantor
X-DocPath=cantor/index.html
MimeType=application/x-cantor;
Categories=Qt;KDE;Education;Math;Science;
Keywords=math;worksheet;notebook;python;r;octave;maxima;calculator;cantor;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.cantor.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/cantor.png
test -e "$PKG"/usr/share/mime/packages/cantor.xml
