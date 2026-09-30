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


# SWIG 4.4 dropped the Python 2 .next() on its iterators, which the pcbnew
# bindings call; without the patch every board iteration from a script raises.
patch -p1 -i "$PORT_SRC/fix-swig-iterator-next.patch"

# Offline: the update check is compiled out and Sentry stays off (it needs a
# DSN). The plugin and content manager is online by nature and simply finds
# nothing; the symbol, footprint and template libraries are ports of their
# own. Translations are not built: bundled data is English only. The GL canvas
# is EGL whenever wx is (wxUSE_GLCANVAS_EGL), and KICAD_WAYLAND adds the
# pointer-constraint and cursor-warp paths. The QA suites are not built.
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

# The generated lexer header is not ordered before its first consumer, so a
# parallel build can compile against a header that does not exist yet.
cmake --build build --target common/pcb_lexer.h
cmake --build build
DESTDIR=$PKG cmake --install build

test -x "$PKG/usr/bin/kicad"
test -x "$PKG/usr/bin/kicad-cli"
test -f "$PKG/usr/share/applications/org.kicad.kicad.desktop"
test -f "$PKG/usr/share/icons/hicolor/48x48/apps/kicad.png"
ls "$PKG"/usr/lib/python3*/site-packages/_pcbnew.so >/dev/null
