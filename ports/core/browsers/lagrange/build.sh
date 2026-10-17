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
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_GUI=ON \
	-DENABLE_TUI=ON \
	-DENABLE_X11_XLIB=OFF \
	-DENABLE_POPUP_MENUS=OFF \
	-DENABLE_RESIZE_DRAW=OFF \
	-DENABLE_FRIBIDI=ON \
	-DENABLE_FRIBIDI_BUILD=OFF \
	-DENABLE_HARFBUZZ=ON \
	-DENABLE_HARFBUZZ_MINIMAL=OFF \
	-DENABLE_MPG123=ON \
	-DENABLE_OPUS=ON \
	-DENABLE_WEBP=ON \
	-DENABLE_JXL=ON \
	-DENABLE_STATIC=OFF \
	-DTFDN_ENABLE_WARN_ERROR=OFF \
	-DTFDN_ENABLE_SSE41=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/fi.skyjake.clagrange.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Lagrange (terminal)
GenericName=Gemini Client
Comment=Browse Gemini, Gopher and Finger in a terminal
Exec=clagrange
Icon=fi.skyjake.clagrange
Terminal=true
Categories=Network;WebBrowser;
Keywords=gemini;gopher;finger;spartan;smolweb;browser;
DESKTOP
chmod 644 "$PKG/usr/share/applications/fi.skyjake.clagrange.desktop"
