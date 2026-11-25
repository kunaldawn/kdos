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

cp -a /usr/share/sddm/themes/breeze/. .
patch -p1 -i "$PORT_SRC/kdos-sddm.patch"
rm -f preview.png default-logo.svg Messages.sh KeyboardButton.qml
install -m644 "$PORT_SRC/sddm-theme.desktop" metadata.desktop
python3 "$PORT_SRC/genpreview.py" "/usr/share/kdos/greeter/$(kdos-theme accents --default)" preview.png

install -d "$PKG/usr/share/sddm/themes/kdos"
cp -a . "$PKG/usr/share/sddm/themes/kdos/"

install -Dm644 /usr/share/licenses/plasma-desktop/LICENSES/LGPL-2.0-or-later.txt \
	"$PKG/usr/share/licenses/$name/LGPL-2.0-or-later.txt"
