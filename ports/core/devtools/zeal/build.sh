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
