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

patch -p1 -i "$PORT_SRC/fix-swig-iterator-next.patch"

cmake -S . -B build -G Ninja -Wno-dev \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DKICAD_UPDATE_CHECK=OFF \
	-DKICAD_USE_SENTRY=OFF \
	-DKICAD_BUILD_I18N=OFF \
	-DKICAD_BUILD_QA_TESTS=OFF \
	-DKICAD_SPICE_QA=OFF \
	-DKICAD_SCRIPTING_WXPYTHON=ON \
	-DKICAD_WAYLAND=ON \
	-DKICAD_IPC_API=ON \
	-DKICAD_SIGNAL_INTEGRITY=ON \
	-DKICAD_IDF_TOOLS=ON \
	-DKICAD_INSTALL_DEMOS=ON \
	-DKICAD_BUILD_FLATPAK=OFF

cmake --build build --target common/pcb_lexer.h
cmake --build build
DESTDIR=$PKG cmake --install build

test -x "$PKG/usr/bin/kicad"
test -x "$PKG/usr/bin/kicad-cli"
test -f "$PKG/usr/share/applications/org.kicad.kicad.desktop"
test -f "$PKG/usr/share/icons/hicolor/48x48/apps/kicad.png"
ls "$PKG"/usr/lib/python3*/site-packages/_pcbnew.so >/dev/null
