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

# the_Foundation, skyjake's own base library, is built from lib/ as a static
# library and not installed; harfbuzz and fribidi come from their ports through
# pkg-config. Each media decoder is pkg-config optional and dropped when
# missing, so every one is in depends. TFDN_ENABLE_WARN_ERROR would turn any new
# compiler warning into a failed build. TFDN_ENABLE_SSE41 is off because the
# x86-64 baseline has no SSE 4.1. ENABLE_X11_XLIB is off: SDL reaches the
# display through Wayland. Popup menus as separate windows are off, so menus
# stay inside the main surface.
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

# Upstream's GUI entry is kept: a 256 px PNG in hicolor and
# StartupWMClass=lagrange, the name SDL gives the window. Its terminal entry is
# REPLACED: it names clagrange by absolute path, which bypasses the launcher's
# terminal choice, it is labelled with the bare program name, and it claims the
# same URL schemes as the GUI, so a gemini: link would open in whichever entry
# sorted first.
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
