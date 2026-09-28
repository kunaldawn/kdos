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

# ZEAL_FEATURE_UPDATE_CHECK is a request to GitHub on every start, so it is
# compiled out. The global show-window shortcut is implemented only through
# XCB and grabs nothing under the Wayland platform this session runs Qt
# applications on, so X11 is not looked for and the no-op backend is built.
# toml++ comes from its port rather than the bundled copy; cpp-httplib, which
# serves docset pages to the web view over loopback, has no port and the
# bundled header is used. Upstream turns every warning into an error, and a
# newer compiler's new warnings would then fail the build, so that is
# switched off from the command line.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D ZEAL_FEATURE_UPDATE_CHECK=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_X11=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_httplib=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_tomlplusplus=ON \
	--compile-no-warning-as-error \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The entry is replaced for StartupWMClass: Zeal sets its desktop file name,
# and so its Wayland app_id, to org.zealdocs.zeal.
cat > "$PKG/usr/share/applications/org.zealdocs.zeal.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Zeal
GenericName=Documentation Browser
Comment=Search API documentation offline
TryExec=zeal
Exec=zeal %u
Icon=zeal
Terminal=false
StartupWMClass=org.zealdocs.zeal
MimeType=x-scheme-handler/dash;x-scheme-handler/dash-plugin;
Categories=Qt;Development;Documentation;
Keywords=documentation;docs;api;reference;docset;offline;zeal;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.zealdocs.zeal.desktop"
