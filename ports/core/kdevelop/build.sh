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

# C and C++ support is libclang's (the clang port's ClangConfig.cmake); the
# plugins that drive clang-tidy, cppcheck, gdb, lldb, CMake, meson, make,
# ninja, git and the Qt Help and man-page documentation are built, and the
# documentation view is a QtWebEngine page. Patch review needs
# libkomparediff2; the QMake builder and manager have their parsers generated
# by KDevelop-PG-Qt; the debuggers' "attach to process" dialog lists processes
# through libksysguard. Each of those is required here rather than silently
# dropped when absent. The astyle formatter is upstream's bundled library.
# Off, each for a reason: the Subversion, Okteta, heaptrack and clazy plugins
# need programs this system does not carry; the plasmoid runner needs Plasma,
# and the GitHub project provider is an online service. KF_SKIP_PO_PROCESSING
# leaves the translation catalogues out: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
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

# kdoctools_install() builds every translated handbook and manual page; only
# the English ones are kept.
for d in "$PKG"/usr/share/doc/HTML/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/HTML/en | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

# THE TWO VISIBLE ENTRIES ARE REPLACED for StartupWMClass: KAboutData sets the
# desktop file name, and so the Wayland app_id, to org.kde.kdevelop; upstream
# writes the X11 class instead. org.kde.kdevelop_kdev4.desktop, the hidden
# handler for .kdev4 project files, is left as upstream ships it.
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
StartupWMClass=org.kde.kdevelop
Categories=Qt;KDE;Development;IDE;
Keywords=ide;session;kdevelop;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/org.kde.kdevelop.desktop \
	"$PKG"/usr/share/applications/org.kde.kdevelop_ps.desktop
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/kdevelop.png
