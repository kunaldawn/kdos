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

# Alpine's two musl patches: musl has no execinfo.h (backtraces on a failed
# assertion) and no malloc_trim (the idle memory release after a project
# closes); each call is compiled out.
patch -p1 -i "$PORT_SRC/no-execinfo.h.patch"
patch -p1 -i "$PORT_SRC/no-malloc_trim.patch"

# The C++ code model and Clang-Tidy/Clazy runner use the clang port's
# libraries at build time and clangd at run time. The help viewer is the
# bundled litehtml backend, so no QtWebEngine; Qt documentation is read from
# installed .qch files. libarchive and yaml-cpp are the system libraries;
# elfutils and zstd give the perf profiler its parser. Rust and D symbol
# demangling in that parser have no library here and are off.
# Off, each for a reason: the cmdbridge helper is Go built for every remote
# platform; the ClangFormat plugin links clang statically in a build mode the
# clang port does not provide; Copilot, Compiler Explorer, GitLab, Axivion,
# Code Paster, the extension store, the Learning page and the update checker
# are online services; QBS and the QML Designer are separate products, each
# a large build of its own. Every translation except English is removed after
# the install: bundled data is English only.
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
	-DBUILD_PLUGIN_CODEPASTER=OFF \
	-DBUILD_PLUGIN_EXTENSIONMANAGER=OFF \
	-DBUILD_PLUGIN_LEARNING=OFF \
	-DBUILD_PLUGIN_UPDATEINFO=OFF \
	-DBUILD_HELPVIEWERBACKEND_QTWEBENGINE=OFF \
	-DHELPVIEWER_DEFAULT_BACKEND=litehtml \
	-DCMAKE_DISABLE_FIND_PACKAGE_sentry=ON \
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
find "$PKG/usr/share/qtcreator/translations" -name '*.qm' ! -name '*_en.qm' -delete

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the application sets its
# desktop file name, and so its Wayland app_id, to org.qt-project.qtcreator;
# upstream writes the X11 class instead. The C and C++ source types it claims
# are also claimed by every other editor here, so the entry claims none and
# mimeapps.list chooses.
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
