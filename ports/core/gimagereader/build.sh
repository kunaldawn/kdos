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

# ENABLE_VERSIONCHECK=0: the startup check asks the project's server for a
# newer release. Scanning goes through SANE, PDF output through PoDoFo and the
# PDF and DjVu input through poppler-qt6 and ddjvuapi.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D INTERFACE_TYPE=qt6 \
	-D ENABLE_VERSIONCHECK=0 \
	-D CMAKE_REQUIRE_FIND_PACKAGE_QuaZip-Qt6=ON \
	-Wno-dev
for m in PODOFO SANE TESSERACT; do
	grep -q "^${m}_FOUND:INTERNAL=1" build/CMakeCache.txt || {
		echo "gimagereader: pkg-config found no $m" >&2
		exit 1
	}
done
cmake --build build
DESTDIR=$PKG cmake --install build

# Bundled data is English only: the catalogues and the translated manuals go.
rm -rf "$PKG/usr/share/locale"
rm -f "$PKG"/usr/share/doc/gimagereader/manual-*.html

# UPSTREAM'S ENTRY IS REPLACED to drop its MimeType line, which claims images,
# PDF and HTML that the viewers and the browser open. The Wayland app_id is
# the executable's name, gimagereader-qt6. The hicolor PNGs it names are
# upstream's, installed above.
cat > "$PKG/usr/share/applications/gimagereader-qt6.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=gImageReader
GenericName=OCR Application
Comment=Recognise text in scans, images and PDFs
Exec=gimagereader-qt6 %F
Icon=gimagereader
Terminal=false
StartupNotify=true
StartupWMClass=gimagereader-qt6
Categories=Qt;Graphics;OCR;Scanning;
Keywords=ocr;optical character recognition;scan;scanner;tesseract;text;
DESKTOP
chmod 644 "$PKG/usr/share/applications/gimagereader-qt6.desktop"
