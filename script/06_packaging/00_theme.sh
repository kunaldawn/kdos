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
# Seed the DEFAULT theme into /etc/skel so a fresh home (live ISO, new user)
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
# THE SHELL'S NAME AND THE COMPILED ONE MUST BE THE SAME ACCENT, and this is
# where that is proved rather than assumed. `kdos-bootctl theme --print` with no
# argument expands libkcolor's own `KCOL_DEFAULT_ID`; with one it expands the
# name it is given. Identical output means `$KDOS_ACCENT` IS the compiled
# default. If they ever diverge the medium seeds one scheme while every
# unconfigured program picks another, and the desktop changes colour the first
# time anything is opened — which reads as a bug in that program.
if ! diff -q <(kdos-bootctl theme --print) \
             <(kdos-bootctl theme --print "$KDOS_ACCENT") >/dev/null; then
    echo "FATAL: KDOS_ACCENT=$KDOS_ACCENT is not libkcolor's KCOL_DEFAULT_ID." >&2
    echo "       Change one of them; they name the same thing." >&2
    exit 1
fi

echo "Seeding $KDOS_ACCENT theme (GTK + icons + foot/btop/starship) into /etc/skel..."
# `kdos theme` is the single generator — running it against skel keeps the
# seeds byte-identical to what a live `kdos theme <accent>` produces. It is
# also what MATERIALIZES ~/.themes/KDOS and ~/.icons/KDOS: the packages only
# install the system copies plus their generators, because the appbox shares
# $HOME and not /usr/share, so the home copies are the ones alien apps see.
HOME=/etc/skel XDG_CONFIG_HOME=/etc/skel/.config XDG_CACHE_HOME=/etc/skel/.cache \
    XDG_DATA_HOME=/etc/skel/.local/share \
    /usr/local/bin/kdos theme "$KDOS_ACCENT"
test -s /etc/skel/.cache/kdos/theme
test -s /etc/skel/.config/gtk-3.0/settings.ini
test -s /etc/skel/.config/gtk-4.0/gtk.css
# The stylesheet's directory carries the accent, because GTK reloads a theme
# when its NAME moves and never when one file under a fixed name is rewritten.
# `KDOS` is a link to it, which is what every seeded settings.ini, XCURSOR path
# and shipped package copy still names.
test -s "/etc/skel/.themes/KDOS-$KDOS_ACCENT/gtk-3.0/gtk.css"
test -L /etc/skel/.themes/KDOS
test -s /etc/skel/.icons/KDOS/index.theme
# The window frames. This file used to ship from fs/ as a fixed neutral grey
# and was the one artefact an accent switch could not reach; it is generated
# now, from the same palette as everything else, and kdos-comp re-reads it on
# the SIGHUP `kdos theme` already sends.
test -s /etc/skel/.config/kdos-comp/themerc-override
# The cursors are generated here too now that kdos-cursors ships its art to
# /usr/share/kdos/cursors/art. The package installs a build of its own
# into /etc/skel/.icons; this rewrites it from the same generator and the same
# art, so the two agree by construction rather than by luck.
test -s /etc/skel/.icons/KDOS-cursors/cursors/default
# The KDE bridge: boxed dolphin/okular/kate read this file out of the shared
# home, and a skel without it hands every new user a grey KDE.
test -s /etc/skel/.config/kdeglobals
test -s /etc/skel/.local/share/color-schemes/KDOS.colors
