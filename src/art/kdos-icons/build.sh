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
	install -d "$ICONS/$theme"
	cp -R "$PORT_SRC/themes/$theme/." "$ICONS/$theme/"
	chmod -R u=rwX,go=rX "$ICONS/$theme"
	for s in 16 22 24 32 48 64 128 256; do
		d="$ICONS/$theme/${s}x${s}/places"
		install -Dm644 "$PORT_SRC/marks/tux-$s.png" "$d/start-here-kde.png"
		for alias in start-here-kde-symbolic start-here-kde-plasma \
			     start-here-kde-plasma-symbolic start-here-kdos; do
			ln -s start-here-kde.png "$d/$alias.png"
		done
	done
done

for pair in KDOS:breeze-dark KDOS-Light:breeze; do
	theme=${pair%%:*} base=${pair#*:}
	install -d "$ICONS/$theme/scalable/apps"
	ln -s "../../../$base/actions/22/internet-services.svg" "$ICONS/$theme/scalable/apps/im-google.svg"
	ln -s "../../../$base/apps/22/kgpg-symbolic.svg" "$ICONS/$theme/scalable/apps/kgpg.svg"
done

install -Dm644 "$PORT_SRC/themes/default/index.theme" "$ICONS/default/index.theme"
