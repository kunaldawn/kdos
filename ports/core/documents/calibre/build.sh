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

# The update and plugin-update checks are patched out. The second patch pins
# the headless QPA plugin's CMake generator, which otherwise follows
# CMAKE_GENERATOR from the environment. The piper extension is dropped: it
# needs espeak-ng's unreleased API and onnxruntime, and its voices are
# downloaded on first use; read-aloud goes through Qt TextToSpeech and
# speech-dispatcher instead.
patch -p1 -i "$PORT_SRC/0001-calibre-no-update.patch"
patch -p1 -i "$PORT_SRC/0002-calibre-use-make.patch"
patch -p1 -i "$PORT_SRC/0003-calibre-disable-piper.patch"

# The release tarball already carries every generated resource the install
# needs: the compiled RapydScript viewer and editor, MathJax, the hyphenation
# dictionaries, the iso-codes tables and the Liberation fonts. `install` runs
# only `build` and `gui`, so nothing below reaches the network. Importing
# calibre during the build writes a configuration directory, kept inside the
# work tree.
export CALIBRE_CONFIG_DIRECTORY="$SRC_ROOT/calibre-config"
export QT_QPA_PLATFORM=offscreen
python3 setup.py install \
	--prefix=/usr \
	--staging-root="$PKG/usr" \
	--no-postinstall

# THE PYTHON MODULES ONLY CALIBRE IMPORTS go into calibre's own library
# directory, which its launchers put first on sys.path, so none of them is a
# path in site-packages that another package could also own. The shared ones
# (lxml, pillow, psutil, pygments, dateutil, six, html5lib, webencodings,
# typing-extensions, fontTools, PyQt6, and Brotli from the brotli port) are
# ports in depends. Build isolation is off and the backends are ports in
# depends: setuptools, wheel, hatchling, flit-core, poetry-core, Cython and
# setuptools-scm. python-xxhash links the system libxxhash and pychm the
# chmlib port. Bytecode is compiled once below, with the package paths.
mkdir -p vendor
tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" --strip-components=1 -C vendor
export XXHASH_LINK_SO=1
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--no-compile --target="$PKG/usr/lib/calibre" \
	css-parser jeepney dnspython mechanize feedparser-sgmllib feedparser \
	markdown html2text soupsieve beautifulsoup4 regex chardet msgpack \
	pycryptodome apsw netifaces ifaddr zeroconf lxml-html-clean xxhash \
	tzlocal pystache pychm py7zr texttable pycryptodomex pyppmd \
	pybcj multivolumefile inflate64 pykakasi jaconv deprecated wrapt \
	"$SRC_ROOT/html5-parser-$_html5parser"
rm -rf "$PKG/usr/lib/calibre/bin"
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

# The entries are written here because upstream's post-install writes them
# through xdg-utils into the running user's menu. Each program calls
# setDesktopFileName with its entry's name, which is the Wayland app_id. The
# viewer alone claims the e-book types; the library and the editor claim none,
# so a book opens in one program and PDF stays with the document viewer.
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
