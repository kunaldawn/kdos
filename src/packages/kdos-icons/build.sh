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

# The generator also installs the KDOS marks — it has to, because
# `kdos theme <accent>` re-runs it against $HOME and must produce a
# complete theme without this kpkgbuild's help.
kdos-theme icons "$PKG/usr/share/icons/KDOS" phosphor \
	--src "$PORT_SRC/art" --marks "$PORT_SRC/marks"

# hicolor gets the marks too, so every lookup path lands on the tux and not
# just the theme-internal one. Any stale SVG from the pixel-art era has to
# go: at a given size the toolkit picks scalable over a fixed-size PNG.
for n in kdos-launcher; do
	rm -f "$PKG/usr/share/icons/hicolor/scalable/apps/$n.svg"
	for f in "$PORT_SRC"/marks/tux-*.png; do
		s="${f##*/tux-}"; s="${s%.png}"
		install -Dm644 "$f" \
			"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/$n.png"
	done
done

# `kdos theme <accent>` re-runs kdos-theme against $HOME — the artwork
# is flat single-fill SVG, so the accent lives in the files and no CSS
# reaches it. That run is also what produces ~/.icons/KDOS, which is the
# only icon path the appbox can see (it shares $HOME, not /usr/share) and
# is why no /etc/skel copy is installed here: 06_packaging/00_theme.sh
# runs `kdos theme phosphor` against /etc/skel and generates it.
# The art and the marks ship so `kdos theme <accent>` can regenerate the whole
# icon set on the running system: kdos-theme falls back to ICON_ART_DEFAULT and
# ICON_MARKS_DEFAULT, which are exactly these two paths. `cp -a` will not create
# the parent, and $PKG is a fresh staging tree for every package, so nothing
# else has made usr/share/kdos yet.
mkdir -p "$PKG/usr/share/kdos/icons"
cp -a "$PORT_SRC/marks" "$PKG/usr/share/kdos/icons/marks"
cp -a "$PORT_SRC/art" "$PKG/usr/share/kdos/icons/art"

# The pixel-icon atlas libkicon reads: the same artwork rasterised on the
# maintainer's host by genatlas.py, in upstream's colours, tinted at load.
# Absent is a working state — the desktop draws its glyph tier — so this is
# conditional rather than a build failure on a tree that has not generated one.
if [ -f "$PORT_SRC/atlas/atlas.kia" ]; then
	install -Dm644 "$PORT_SRC/atlas/atlas.kia" \
		"$PKG/usr/share/kdos/icons/atlas.kia"
fi

install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/kdos-icons/LICENSE.notice"
install -Dm644 "$PORT_SRC/art/LICENSE.upstream" \
	"$PKG/usr/share/licenses/kdos-icons/LICENSE.papirus"
