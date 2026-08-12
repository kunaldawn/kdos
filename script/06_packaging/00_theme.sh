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

# The COSMIC generators are gone, and the state they wrote into /etc/skel is
# NOT removable by the fs-manifest guard: that guard only owns paths fs/ once
# provided, and these were generated here at packaging time by
# kdos-theme-helper and by kdos.c's write_panel_colors(). The seeds fs/ did
# provide were removed correctly on the first re-sync; these 60-odd files
# would otherwise ride every future ISO exactly the way the stale icon did.
# Idempotent, so it stays rather than being a one-off cleanup someone has to
# remember.
echo "Seeding PHOSPHOR theme (GTK + icons + foot/btop/starship) into /etc/skel..."
# `kdos theme` is the single generator — running it against skel keeps the
# seeds byte-identical to what a live `kdos theme phosphor` produces. It is
# also what MATERIALIZES ~/.themes/KDOS and ~/.icons/KDOS: the packages only
# install the system copies plus their generators, because the appbox shares
# $HOME and not /usr/share, so the home copies are the ones alien apps see.
HOME=/etc/skel XDG_CONFIG_HOME=/etc/skel/.config XDG_CACHE_HOME=/etc/skel/.cache \
    XDG_DATA_HOME=/etc/skel/.local/share \
    /usr/local/bin/kdos theme phosphor
test -s /etc/skel/.cache/kdos/theme
test -s /etc/skel/.config/gtk-3.0/gtk.css
test -s /etc/skel/.config/gtk-4.0/gtk.css
test -s /etc/skel/.themes/KDOS/gtk-3.0/gtk.css
test -s /etc/skel/.icons/KDOS/index.theme
# The cursors are generated here too now that kdos-cursors ships its art to
# /usr/share/kdos/cursors/art. The package installs a phosphor build of its own
# into /etc/skel/.icons; this rewrites it from the same generator and the same
# art, so the two agree by construction rather than by luck.
test -s /etc/skel/.icons/KDOS-cursors/cursors/default
# The KDE bridge: boxed dolphin/okular/kate read this file out of the shared
# home, and a skel without it hands every new user a grey KDE.
test -s /etc/skel/.config/kdeglobals
test -s /etc/skel/.local/share/color-schemes/KDOS.colors
