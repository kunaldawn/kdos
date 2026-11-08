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

ICONS="$PKG/usr/share/icons"

for s in 16 22 24 32 48 64 128 256; do
	install -Dm644 "$PORT_SRC/marks/tux-$s.png" \
		"$ICONS/hicolor/${s}x${s}/apps/start-here-kdos.png"
	install -Dm644 "$PORT_SRC/marks/logo-$s.png" \
		"$ICONS/hicolor/${s}x${s}/apps/distributor-logo-kdos.png"
	install -Dm644 "$PORT_SRC/marks/logo-$s.png" \
		"$ICONS/hicolor/${s}x${s}/apps/kdos.png"
done

for theme in KDOS KDOS-Light; do
	install -Dm644 "$PORT_SRC/themes/$theme/index.theme" "$ICONS/$theme/index.theme"
	for s in 16 22 24 32 48 64 128 256; do
		d="$ICONS/$theme/${s}x${s}/places"
		install -Dm644 "$PORT_SRC/marks/tux-$s.png" "$d/start-here-kde.png"
		for alias in start-here-kde-symbolic start-here-kde-plasma \
			     start-here-kde-plasma-symbolic; do
			ln -s start-here-kde.png "$d/$alias.png"
		done
	done
done

install -Dm644 "$PORT_SRC/themes/default/index.theme" "$ICONS/default/index.theme"
