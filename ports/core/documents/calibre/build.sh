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

patch -p1 -i "$PORT_SRC/0001-calibre-no-update.patch"
patch -p1 -i "$PORT_SRC/0002-calibre-use-make.patch"
patch -p1 -i "$PORT_SRC/0003-calibre-disable-piper.patch"

mkdir -p vendor
tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" --strip-components=1 -C vendor
tar -xf vendor/apsw-*.tar.gz -C "$SRC_ROOT"
_apsw=$(echo "$SRC_ROOT"/apsw-*/)
rm "$_apsw/setup.apsw"
tar -xf vendor/netifaces-*.tar.gz -C "$SRC_ROOT"
_netifaces=$(echo "$SRC_ROOT"/netifaces-*/)
patch -d "$_netifaces" -p1 -i "$PORT_SRC/netifaces-msghdr.patch"
export XXHASH_LINK_SO=1
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--no-compile --target="$SRC_ROOT/calibre-modules" \
	css-parser jeepney dnspython mechanize feedparser-sgmllib feedparser \
	markdown html2text soupsieve beautifulsoup4 regex chardet msgpack \
	pycryptodome "$_apsw" "$_netifaces" ifaddr zeroconf lxml-html-clean xxhash \
	tzlocal pystache pychm py7zr texttable pycryptodomex pyppmd \
	pybcj multivolumefile inflate64 pykakasi jaconv deprecated wrapt \
	"$SRC_ROOT/html5-parser-$_html5parser"
rm -rf "$SRC_ROOT/calibre-modules/bin"

export CALIBRE_CONFIG_DIRECTORY="$SRC_ROOT/calibre-config"
export QT_QPA_PLATFORM=offscreen
CFLAGS="${CFLAGS/-std=gnu11/}" PYTHONPATH="$SRC_ROOT/calibre-modules" python3 setup.py install \
	--prefix=/usr \
	--staging-root="$PKG/usr" \
	--no-postinstall
cp -a "$SRC_ROOT/calibre-modules/." "$PKG/usr/lib/calibre/"

python3 -m compileall -q -j1 -s "$PKG" -p / "$PKG/usr/lib/calibre"

install -Dm644 -t "$PKG/usr/share/man/man1" man-pages/man1/*.1
install -Dm644 resources/calibre-mimetypes.xml \
	"$PKG/usr/share/mime/packages/calibre-mimetypes.xml"
install -Dm644 resources/images/lt.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/calibre-gui.png"
install -Dm644 resources/images/viewer.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/calibre-viewer.png"
install -Dm644 resources/images/tweak.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/calibre-ebook-edit.png"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/calibre-gui.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=calibre
GenericName=E-book Library
Comment=Organise, convert and send e-books
TryExec=calibre
Exec=calibre --detach %U
Icon=calibre-gui
Terminal=false
StartupWMClass=calibre-gui
MimeType=x-scheme-handler/calibre;
Categories=Qt;Office;
Keywords=epub;ebook;library;convert;manager;kindle;
DESKTOP
cat > "$PKG/usr/share/applications/calibre-ebook-viewer.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=E-book Viewer
GenericName=E-book Reader
Comment=Read EPUB, MOBI, AZW3, FB2 and comic books
TryExec=ebook-viewer
Exec=ebook-viewer --detach %f
Icon=calibre-viewer
Terminal=false
StartupWMClass=calibre-ebook-viewer
MimeType=application/epub+zip;application/x-mobipocket-ebook;application/x-mobi8-ebook;application/vnd.amazon.mobi8-ebook;application/x-fictionbook+xml;application/x-zip-compressed-fb2;application/vnd.comicbook+zip;application/x-sony-bbeb;
Categories=Qt;Office;Viewer;
Keywords=epub;ebook;reader;viewer;mobi;azw3;fb2;
DESKTOP
cat > "$PKG/usr/share/applications/calibre-ebook-edit.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=E-book Editor
GenericName=E-book Editor
Comment=Edit the text and styles inside EPUB and AZW3 books
TryExec=ebook-edit
Exec=ebook-edit --detach %f
Icon=calibre-ebook-edit
Terminal=false
StartupWMClass=calibre-ebook-edit
Categories=Qt;Office;WordProcessor;
Keywords=epub;ebook;editor;azw3;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/calibre-*.desktop
