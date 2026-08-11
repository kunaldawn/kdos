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
#
# Seed the PHOSPHOR theme into /etc/skel so a fresh home (live ISO, new user)
# boots straight into the KDOS look. Runs before 00_user.sh materializes homes
# (lexicographic order does the sequencing). The palette itself is libkcolor's
# — kdos-comp and kdos-shell link it and read only the accent NAME from
# $XDG_CACHE_HOME/kdos/theme, so nothing here writes colours for the desktop.
# Everything below is for software that is not ours: GTK, foot, btop, starship.

set -e
source script/packaging.env.sh

echo "Seeding PHOSPHOR theme (GTK + icons + foot/btop/starship) into /etc/skel..."
# `kdos theme` is the single generator — running it against skel keeps the
# seeds byte-identical to what a live `kdos theme phosphor` produces. It is
# also what MATERIALIZES ~/.themes/KDOS and ~/.icons/KDOS: the packages only
# install the system copies plus their generators, because the appbox shares
# $HOME and not /usr/share, so the home copies are the ones alien apps see.
HOME=/etc/skel XDG_CONFIG_HOME=/etc/skel/.config XDG_CACHE_HOME=/etc/skel/.cache \
    /usr/local/bin/kdos theme phosphor
test -s /etc/skel/.cache/kdos/theme
test -s /etc/skel/.config/gtk-3.0/gtk.css
test -s /etc/skel/.config/gtk-4.0/gtk.css
test -s /etc/skel/.themes/KDOS/gtk-3.0/gtk.css
test -s /etc/skel/.icons/KDOS/index.theme
