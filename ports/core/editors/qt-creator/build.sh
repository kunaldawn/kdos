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

patch -p1 -i "$PORT_SRC/no-execinfo.h.patch"
patch -p1 -i "$PORT_SRC/no-malloc_trim.patch"

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_WITH_PCH=OFF \
	-DWITH_TESTS=OFF \
	-DWITH_DOCS=OFF \
	-DBUILD_QBS=OFF \
	-DWITH_QMLDESIGNER=OFF \
	-DBUILD_DESIGNSTUDIO=OFF \
	-DQTC_USE_INTERNAL_TASKTREE=ON \
	-DQTC_CLANG_BUILDMODE_MATCH=ON \
	-DBUILD_EXECUTABLE_CMDBRIDGE=OFF \
	-DBUILD_PLUGIN_CLANGFORMAT=OFF \
	-DBUILD_PLUGIN_COPILOT=OFF \
	-DBUILD_PLUGIN_COMPILEREXPLORER=OFF \
	-DBUILD_PLUGIN_GITLAB=OFF \
	-DBUILD_PLUGIN_AXIVION=OFF \
	-DBUILD_PLUGIN_EXTENSIONMANAGER=OFF \
	-DBUILD_PLUGIN_LEARNING=OFF \
	-DBUILD_PLUGIN_UPDATEINFO=OFF \
	-DBUILD_HELPVIEWERBACKEND_QTWEBENGINE=OFF \
	-DHELPVIEWER_DEFAULT_BACKEND=litehtml \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Clang=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibArchive=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_yaml-cpp=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_elfutils=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Zstd=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LibRustcDemangle=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LibDDemangle=ON
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/qtcreator"
ls "$PKG"/usr/lib/qtcreator/plugins/libClangCodeModel.so >/dev/null

cat > "$PKG/usr/share/applications/org.qt-project.qtcreator.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Qt Creator
GenericName=C++ and Qt IDE
Comment=Develop C++, QML and Python applications with Qt
TryExec=qtcreator
Exec=qtcreator %F
Icon=QtProject-qtcreator
Terminal=false
StartupWMClass=org.qt-project.qtcreator
Categories=Development;IDE;Qt;
Keywords=ide;c++;qt;qml;cmake;debugger;designer;qtcreator;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.qt-project.qtcreator.desktop"
test -e "$PKG"/usr/share/icons/hicolor/48x48/apps/QtProject-qtcreator.png
