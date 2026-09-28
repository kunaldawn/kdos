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

# Blend2D and its JIT backend AsmJit are FetchContent git clones at the
# commits the project pins; the build has no network, so each is a later
# source and FETCHCONTENT_SOURCE_DIR_* hands CMake the unpacked tree in place
# of the clone. Blend2D is linked statically, as upstream builds it.
# USE_QT6DBus is device detection: a Garmin or a phone mounted by udisks2
# appears in the workspace. Translations are compiled into the program by
# qt_add_translations and have no switch.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTING=OFF \
	-DBUILD_QMAPSHACK=ON \
	-DBUILD_QMAPTOOL=ON \
	-DUSE_QT6DBus=ON \
	-DBUILD_FOR_LOCAL_SYSTEM=OFF \
	-DDEVELOPMENT_VERSION=OFF \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DFETCHCONTENT_SOURCE_DIR_ASMJIT="$SRC_ROOT/asmjit-$_asmjit" \
	-DFETCHCONTENT_SOURCE_DIR_BLEND2D="$SRC_ROOT/blend2d-$_blend2d"
cmake --build build
DESTDIR=$PKG cmake --install build
for f in applications/qmapshack.desktop applications/qmaptool.desktop; do
	[ -f "$PKG/usr/share/$f" ] || { echo "qmapshack: $f was not installed" >&2; exit 1; }
done
ls "$PKG"/usr/share/icons/hicolor/*/apps/QMapShack.png >/dev/null ||
	{ echo "qmapshack: no hicolor PNG named QMapShack" >&2; exit 1; }
