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

patch -p1 -i "$PORT_SRC/no-telemetry-no-update-check.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D QON_QT6_BUILD=ON \
	-D BUILD_WITH_SYSTEM_BOTAN=OFF \
	-D BUILD_WITH_LIBGIT2=OFF \
	-D BUILD_WITH_ASPELL=OFF \
	-D USE_QLITEHTML=ON \
	-Wno-dev
grep -q '^LIBSECRET_FOUND:INTERNAL=1' build/CMakeCache.txt || {
	echo 'qownnotes: libsecret-1 was not found; passwords could not be stored' >&2
	exit 1
}
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/QOwnNotes/translations"

rm -f "$PKG/usr/share/applications/PBE.QOwnNotes.desktop"
cat > "$PKG/usr/share/applications/PBE.QOwnNotes.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=QOwnNotes
GenericName=Markdown Notes
Comment=Plain-text Markdown notes with a live preview
Exec=QOwnNotes
Icon=QOwnNotes
Terminal=false
StartupNotify=true
StartupWMClass=PBE.QOwnNotes
Categories=Qt;Utility;TextEditor;
Keywords=markdown;notes;todo;journal;nextcloud;owncloud;
DESKTOP
chmod 644 "$PKG/usr/share/applications/PBE.QOwnNotes.desktop"
