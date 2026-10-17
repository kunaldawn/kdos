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

printf 'enable_language(C)\n' > "$SRC_ROOT/qmapshack-c.cmake"
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_QMAPSHACK=ON \
	-DBUILD_QMAPTOOL=ON \
	-DUSE_QT6DBus=ON \
	-DBUILD_FOR_LOCAL_SYSTEM=OFF \
	-DDEVELOPMENT_VERSION=OFF \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBLEND2D_NO_INSTALL=ON \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DFETCHCONTENT_SOURCE_DIR_ASMJIT="$SRC_ROOT/asmjit-$_asmjit" \
	-DFETCHCONTENT_SOURCE_DIR_BLEND2D="$SRC_ROOT/blend2d-$_blend2d" \
	-DCMAKE_PROJECT_QMapShack_INCLUDE="$SRC_ROOT/qmapshack-c.cmake"
cmake --build build
DESTDIR=$PKG cmake --install build
for f in applications/qmapshack.desktop applications/qmaptool.desktop; do
	[ -f "$PKG/usr/share/$f" ] || { echo "qmapshack: $f was not installed" >&2; exit 1; }
done
ls "$PKG"/usr/share/icons/hicolor/*/apps/QMapShack.png >/dev/null ||
	{ echo "qmapshack: no hicolor PNG named QMapShack" >&2; exit 1; }
