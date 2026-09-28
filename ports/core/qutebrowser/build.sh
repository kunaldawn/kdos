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

# StartupWMClass follows --desktop-file-name, whose default
# org.qutebrowser.qutebrowser is the Wayland app_id. The MimeType line keeps
# only qute://: Firefox ESR's entry is the web and image handler, and a second
# claim would make the default whichever entry sorts first.
patch -p1 -i "$PORT_SRC/app-id.patch"

# The in-program help under qutebrowser/html/doc is prebuilt in the release
# tarball and goes into the wheel with the package. The optional adblock
# module (Brave's Rust engine) and tldextract are not ports: the host-list
# blocker is the one that works, and its lists are fetched only when the user
# runs :adblock-update.
python3 -m pip install --no-build-isolation --no-deps --root "$PKG" --prefix /usr .

# misc/Makefile's install target runs `setup.py install`; the files it adds
# beside the package are installed here instead. The manual page is prebuilt
# in the release tarball, so a2x and its DocBook toolchain are not needed.
# scripts/ is installed as upstream does, less mkvenv.py, which builds a
# development virtualenv from the package index.
install -Dm644 doc/qutebrowser.1 "$PKG/usr/share/man/man1/qutebrowser.1"
install -Dm644 misc/org.qutebrowser.qutebrowser.desktop \
	"$PKG/usr/share/applications/org.qutebrowser.qutebrowser.desktop"
install -Dm644 misc/org.qutebrowser.qutebrowser.appdata.xml \
	"$PKG/usr/share/metainfo/org.qutebrowser.qutebrowser.appdata.xml"
for s in 16 24 32 48 64 128 256 512; do
	install -Dm644 "qutebrowser/icons/qutebrowser-${s}x${s}.png" \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/qutebrowser.png"
done
install -Dm644 qutebrowser/icons/qutebrowser.svg \
	"$PKG/usr/share/icons/hicolor/scalable/apps/qutebrowser.svg"
install -d "$PKG/usr/share/qutebrowser/userscripts" "$PKG/usr/share/qutebrowser/scripts"
for f in misc/userscripts/*; do
	[ -f "$f" ] && install -m755 "$f" "$PKG/usr/share/qutebrowser/userscripts/"
done
for f in cycle-inputs.js dictcli.py hist_importer.py hostblock_blame.py \
	importer.py keytester.py open_url_in_instance.sh opengl_info.py utils.py; do
	install -m755 "scripts/$f" "$PKG/usr/share/qutebrowser/scripts/"
done
