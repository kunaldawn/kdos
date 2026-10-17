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
	-D CMAKE_C_STANDARD=23 \
	-D CMAKE_C_EXTENSIONS=ON \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_BENCHMARKS=OFF \
	-D BUILD_DOC=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_executeplasmoid=OFF \
	-D BUILD_ghprovider=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Clang=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KompareDiff2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Help=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Purpose=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Boost=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KSysGuard=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KDevelopPGQt=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_SubversionLibrary=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_OktetaKastenControllers=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KastenControllers=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_OktetaGui=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_heaptrack=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_heaptrack_gui=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_ClazyStandalone=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/kdevelop"
ls "$PKG"/usr/lib/qt6/plugins/kdevplatform/*/kdevclangsupport.so >/dev/null

cat > "$PKG/usr/share/applications/org.kde.kdevelop.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KDevelop
GenericName=Integrated Development Environment
Comment=C and C++ development with code analysis, CMake and a debugger
TryExec=kdevelop
Exec=kdevelop
Icon=kdevelop
Terminal=false
StartupWMClass=org.kde.kdevelop
X-DocPath=kdevelop/index.html
Categories=Qt;KDE;Development;IDE;
Keywords=ide;c++;cmake;debugger;clang;programming;code;kdevelop;
Actions=OpenSession;

[Desktop Action OpenSession]
Name=Open a Session
Exec=kdevelop --ps
DESKTOP
cat > "$PKG/usr/share/applications/org.kde.kdevelop_ps.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KDevelop (Pick Session)
GenericName=Integrated Development Environment
Comment=Choose a KDevelop session to start with
TryExec=kdevelop
Exec=kdevelop --ps
Icon=kdevelop
Terminal=false
NoDisplay=true
StartupWMClass=org.kde.kdevelop
Categories=Qt;KDE;Development;IDE;
Keywords=ide;session;kdevelop;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/org.kde.kdevelop.desktop \
	"$PKG"/usr/share/applications/org.kde.kdevelop_ps.desktop
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/kdevelop.png
