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

# SCPI OVER ETHERNET IS HOW A MODERN INSTRUMENT IS SCRIPTED. A scope or a
# supply made in the last fifteen years answers text commands on port 5025 —
# `lxi scpi "MEAS:VOLT:DC?"` — and `lxi discover` finds them by mDNS, which
# avahi is already running for. That turns a bench into something a shell
# script can sweep, log and plot with gnuplot.
#
# -Dgui=true is lxi-gui, on GTK 4 and libadwaita: discovery, a SCPI console,
# screenshots from the instrument and live charts of what it measures. Every
# GUI dependency names a meson subproject as its fallback, and
# --wrap-mode=nofallback keeps a missing library a configure error instead of
# a download. Its GSettings schema is compiled by kpkg's shared-index step.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	--wrap-mode=nofallback \
	-Dbashcompletiondir=/usr/share/bash-completion/completions \
	-Dgui=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# The panel draws only PNG icons, and upstream installs the logo as SVG.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s \
		data/icons/hicolor/scalable/apps/io.github.lxi-tools.lxi-gui.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/io.github.lxi-tools.lxi-gui.png"
done

# Upstream's entry is named after the binary and has no StartupWMClass.
# GtkApplication makes the app_id io.github.lxi-tools.lxi-gui, which the entry
# repeats.
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
