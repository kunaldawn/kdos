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

LOOK="$PKG/usr/share"
mapfile -t accents < <(kdos-theme accents)

kdos-theme kde-colors "$LOOK/color-schemes"
kdos-theme contrast -q "$LOOK"/color-schemes/KDOS*.colors
kdos-theme konsole "$LOOK/konsole"
kdos-theme qt5ct-colors "$LOOK/qt5ct/colors"
kdos-theme greeter-globals --sddm "$LOOK/kdos/greeter"

install -Dm644 "$PORT_SRC/xdg/kdeglobals" "$LOOK/kdos/xdg/kdeglobals"
kdos-theme greeter-globals "$LOOK/kdos/xdg/kdeglobals"
kdos-theme contrast -q "$LOOK/kdos/xdg/kdeglobals"

python3 "$PORT_SRC/genlook.py" \
	--src "$PORT_SRC" \
	--pkg "$PKG" \
	--schemes "$LOOK/color-schemes" \
	--splash "$PORT_SRC/../../art/kdos-splash" \
	--accents "${accents[@]}"
