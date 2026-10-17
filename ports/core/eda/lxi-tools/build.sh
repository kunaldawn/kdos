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

meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	--wrap-mode=nofallback \
	-Dbashcompletiondir=/usr/share/bash-completion/completions \
	-Dgui=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s \
		data/icons/hicolor/scalable/apps/io.github.lxi-tools.lxi-gui.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/io.github.lxi-tools.lxi-gui.png"
done

cat > "$PKG/usr/share/applications/io.github.lxi-tools.lxi-gui.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LXI Instruments
GenericName=Instrument Control
Comment=Find, command and chart bench instruments over the network with SCPI
Exec=lxi-gui
Icon=io.github.lxi-tools.lxi-gui
Terminal=false
StartupWMClass=io.github.lxi-tools.lxi-gui
Categories=GTK;Development;Electronics;
Keywords=lxi;scpi;oscilloscope;multimeter;power supply;instrument;bench;
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.github.lxi-tools.lxi-gui.desktop"
