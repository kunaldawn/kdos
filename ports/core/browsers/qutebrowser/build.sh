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

patch -p1 -i "$PORT_SRC/app-id.patch"

python3 -m pip install --ignore-installed --no-build-isolation --no-deps --root "$PKG" --prefix /usr .

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
