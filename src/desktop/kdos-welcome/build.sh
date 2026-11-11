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

WELCOME="$PKG/usr/share/plasma/plasma-welcome"

install -Dm644 "$PORT_SRC/intro-customization.desktop" "$WELCOME/intro-customization.desktop"
for f in "$PORT_SRC"/extra-pages/*.qml "$PORT_SRC"/extra-pages/kdos/*.qml; do
	install -Dm644 "$f" "$WELCOME/${f#"$PORT_SRC"/}"
done
install -Dm644 "$PORT_SRC/extra-pages/kdos/qmldir" "$WELCOME/extra-pages/kdos/qmldir"
