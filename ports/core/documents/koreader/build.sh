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

tar xf "$PORT_SRC/koreader-thirdparty-$version.tar.xz"

patch -p1 -i "$PORT_SRC/koreader-sh-enable-ko-multiuser.patch"
patch -p1 -i "$PORT_SRC/koreader-sdl-app-id.patch"

unset PKG_CONFIG_PATH
make TARGET= KODEBUG= VERBOSE= TARGET_CFLAGS= \
	QFLAGS=-fstack-clash-protection COMPAT_LDFLAGS=-Wl,-z,now

install -d "$PKG/usr/lib" "$PKG/usr/bin"
cp -RL koreader-emulator-*/koreader "$PKG/usr/lib/koreader"
ln -s ../lib/koreader/koreader.sh "$PKG/usr/bin/koreader"
find "$PKG" -name '*.dbg' -delete
rm -rf "$PKG/usr/lib/koreader/spec"
echo "v$version" > "$PKG/usr/lib/koreader/git-rev"

find "$PKG/usr/lib/koreader/l10n" -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} +

install -Dm644 platform/linux/koreader.1 "$PKG/usr/share/man/man1/koreader.1"

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s resources/koreader.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/rocks.koreader.KOReader.png"
done

install -d "$PKG/usr/share/applications"
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
