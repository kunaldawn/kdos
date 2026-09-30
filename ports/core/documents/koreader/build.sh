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

# THE FIRST SOURCE IS THE SUPERPROJECT WITH ITS SUBMODULES, as Alpine mirrors
# it: base, crengine, fonts and the translations. base then builds some sixty
# third-party libraries (LuaJIT, MuPDF, crengine's, djvulibre, HarfBuzz,
# SQLite and the rest) at pinned versions, each downloaded at build time into
# base/thirdparty/<project>/build/downloads: a tarball checked by MD5, or a
# git clone checked out at a pinned revision.
#
# THE SECOND IS THOSE DOWNLOADS, AND IT IS MADE BY HAND. No tool here writes
# it: it is the tree after `make TARGET= KODEBUG= download-all` (every
# project, device-only ones included), with every build/downloads directory
# packed as ports/fetch packs a bundle, the test fonts, the tessdata model
# and the lock files left out. With each archive present and each clone
# holding its revision, the download step reaches nothing and git checks the
# revisions out offline. Its sha256 line is its identity.
tar xf "$PORT_SRC/koreader-thirdparty-$version.tar.xz"

# KO_MULTIUSER: settings and history go to ~/.config/koreader instead of
# the read-only install directory.
patch -p1 -i "$PORT_SRC/koreader-sh-enable-ko-multiuser.patch"
# SDL_APP_ID: the program SDL runs in is luajit, whose name would otherwise
# be the window's app_id.
patch -p1 -i "$PORT_SRC/koreader-sdl-app-id.patch"

# TARGET empty is the desktop ("emulator") build; KODEBUG empty is release.
# The libraries base builds are private to KOReader and install under
# /usr/lib/koreader; the window and audio come from the system's SDL3.
make TARGET= KODEBUG= VERBOSE=

install -d "$PKG/usr/lib" "$PKG/usr/bin"
cp -RL koreader-emulator-*/koreader "$PKG/usr/lib/koreader"
ln -s ../lib/koreader/koreader.sh "$PKG/usr/bin/koreader"
find "$PKG" -name '*.dbg' -delete
echo "v$version" > "$PKG/usr/lib/koreader/git-rev"

# English only: every translation is a directory under l10n, read as .po at
# run time; the interface falls back to its built-in English strings.
find "$PKG/usr/lib/koreader/l10n" -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} +

install -Dm644 platform/linux/koreader.1 "$PKG/usr/share/man/man1/koreader.1"

# THE MENU ICON, rasterised from upstream's SVG.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s resources/koreader.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/rocks.koreader.KOReader.png"
done

# UPSTREAM'S ENTRY IS REPLACED: it claims PDF, PNG, ZIP, HTML and plain text
# among forty types that belong to other programs, so the entry claims none;
# mimeapps.list chooses the e-book reader.
cat > "$PKG/usr/share/applications/rocks.koreader.KOReader.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KOReader
GenericName=E-book Reader
Comment=Read EPUB, PDF, DjVu, FB2, MOBI and comic books
TryExec=koreader
Exec=koreader %f
Icon=rocks.koreader.KOReader
Terminal=false
StartupWMClass=rocks.koreader.KOReader
Categories=Office;Viewer;
Keywords=ebook;epub;pdf;djvu;fb2;mobi;comic;cbz;reader;book;
DESKTOP
chmod 644 "$PKG/usr/share/applications/rocks.koreader.KOReader.desktop"
