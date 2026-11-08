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

LIBS="$PORT_SRC/../../libs"

python3 -I "$PORT_SRC/genaddons.py" --kcolor "$LIBS/libkcolor/kcolor.h" \
	--palettes "$PORT_SRC/palettes.txt" --out "$PKG/usr/share"

install_tree() {
	(cd "$1" && find . -type f) | while IFS= read -r _f; do
		install -Dm644 "$1/${_f#./}" "$2/${_f#./}"
	done
}

for _d in "$PORT_SRC"/kwin/*/; do
	_d=${_d%/}
	install_tree "$_d" "$PKG/usr/share/kwin/scripts/${_d##*/}"
done
for _d in "$PORT_SRC"/plasmoids/*/; do
	_d=${_d%/}
	install_tree "$_d" "$PKG/usr/share/plasma/plasmoids/${_d##*/}"
done

install -Dm644 "$PORT_SRC/kdos-addons.notifyrc" \
	"$PKG/usr/share/knotifications6/kdos-addons.notifyrc"
install -Dm644 "$PORT_SRC/io.kdos.addons.metainfo.xml" \
	"$PKG/usr/share/metainfo/io.kdos.addons.metainfo.xml"
install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/kdos-addons/LICENSE.notice"
