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

patch -p1 -i "$PORT_SRC/no-update-check.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D QT_VERSION_MAJOR=6 \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Hunspell=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_QuaZip-Qt6=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_QTermWidget=ON \
	-D TEXSTUDIO_ENABLE_MEDIAPLAYER=ON \
	-D TEXSTUDIO_ENABLE_TESTS=OFF \
	-Wno-dev
grep -q '^POPPLER_FOUND:INTERNAL=1' build/CMakeCache.txt || {
	echo 'texstudio: poppler-qt6 was not found; the PDF viewer would be missing' >&2
	exit 1
}
cmake --build build
DESTDIR=$PKG cmake --install build

for f in "$PKG"/usr/share/texstudio/*; do
	[ -f "$f" ] || continue
	case $(basename "$f") in
	*.qm) rm -f "$f" ;;
	en_* | th_en_* | hyph_en_* | README_others.txt) ;;
	*.aff | *.dic | *.stopWords* | *.badWords | th_* | hyph_* | README_*) rm -f "$f" ;;
	esac
done

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s -o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/texstudio.png" \
		utilities/texstudio.svg
done

cat > "$PKG/usr/share/applications/texstudio.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=TeXstudio
GenericName=LaTeX Editor
Comment=Write LaTeX documents with a live PDF preview
Exec=texstudio %F
Icon=texstudio
Terminal=false
StartupNotify=false
StartupWMClass=texstudio
MimeType=text/x-tex;
Categories=Qt;Office;Publishing;
Keywords=tex;latex;pdflatex;xelatex;lualatex;bibtex;editor;
DESKTOP
chmod 644 "$PKG/usr/share/applications/texstudio.desktop"
