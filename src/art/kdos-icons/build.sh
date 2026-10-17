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

kdos-theme icons "$PKG/usr/share/icons/KDOS" phosphor \
	--src "$PORT_SRC/art" --marks "$PORT_SRC/marks"

install_mark() {
	_src=$1; _name=$2
	rm -f "$PKG/usr/share/icons/hicolor/scalable/apps/$_name.svg"
	for f in "$PORT_SRC"/marks/"$_src"-*.png; do
		[ -e "$f" ] || continue
		s="${f##*/$_src-}"; s="${s%.png}"
		install -Dm644 "$f" \
			"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/$_name.png"
	done
}
install_mark tux  kdos-launcher
install_mark logo start-here
install_mark logo distributor-logo-kdos

mkdir -p "$PKG/usr/share/kdos/icons"
cp -a "$PORT_SRC/marks" "$PKG/usr/share/kdos/icons/marks"
cp -a "$PORT_SRC/art" "$PKG/usr/share/kdos/icons/art"

if [ -f "$PORT_SRC/atlas/atlas.kia" ]; then
	install -Dm644 "$PORT_SRC/atlas/atlas.kia" \
		"$PKG/usr/share/kdos/icons/atlas.kia"
fi

install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/kdos-icons/LICENSE.notice"
install -Dm644 "$PORT_SRC/art/LICENSE.upstream" \
	"$PKG/usr/share/licenses/kdos-icons/LICENSE.papirus"
