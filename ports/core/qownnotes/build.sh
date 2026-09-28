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

# The Piwik usage metrics, the heartbeat and the automatic update checks are
# patched out; Help > Check for updates still asks when the user chooses to.
patch -p1 -i "$PORT_SRC/no-telemetry-no-update-check.patch"

# Botan, QtKeychain, FakeVim, litehtml, md4c and Sonnet's hunspell are
# compiled from the copies under libraries/: the build has no switch to take
# any of them from the system except Botan 3, which is not a port. libsecret is
# looked up with no switch, and without it QtKeychain cannot reach the Secret
# Service, so the check after configure refuses a build that missed it.
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

# Bundled data is English only, and English is the untranslated source text.
rm -rf "$PKG/usr/share/QOwnNotes/translations"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: setDesktopFileName makes
# the Wayland app_id PBE.QOwnNotes, not upstream's "QOwnNotes". The hicolor
# PNGs it names are upstream's, installed above.
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
