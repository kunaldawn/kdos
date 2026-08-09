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

# The generator also recolours COSMIC's own app icons and installs the
# KDOS marks — it has to, because `kdos theme <accent>` re-runs it
# against $HOME and must produce a complete theme without this
# kpkgbuild's help.
kdos-theme icons "$PKG/usr/share/icons/KDOS" phosphor \
	--src "$PORT_SRC/art" --marks "$PORT_SRC/marks"

# hicolor gets the marks too, so every lookup path lands on the tux and not
# just the theme-internal one. Any stale SVG from the pixel-art era has to
# go: at a given size the toolkit picks scalable over a fixed-size PNG.
for n in com.system76.CosmicAppLibrary com.system76.CosmicPanelAppButton; do
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
cp -a "$PORT_SRC/marks" "$PKG/usr/share/kdos/icons/marks"
cp -a "$PORT_SRC/art" "$PKG/usr/share/kdos/icons/art"

install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/kdos-icons/LICENSE.notice"
install -Dm644 "$PORT_SRC/art/LICENSE.upstream" \
	"$PKG/usr/share/licenses/kdos-icons/LICENSE.papirus"
