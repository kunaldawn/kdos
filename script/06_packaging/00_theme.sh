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
# Seed the PHOSPHOR COSMIC theme into /etc/skel so a fresh home (live ISO,
# new user) boots straight into the KDOS look. kdos-theme-helper writes the
# applied Dark theme + builder via cosmic-theme's own derivation; runs before
# 00_user.sh materializes homes (lexicographic order does the sequencing).
# Palette must match `kdos theme` phosphor in fs/usr/local/bin/kdos.

set -e
source script/packaging.env.sh

# kdos theme itself degrades gracefully when kdos-theme-helper is missing
# (warns, keeps generating the GTK/foot/btop/starship pieces).
command -v kdos-theme-helper >/dev/null 2>&1 || \
    echo "Warning: kdos-theme-helper not installed — COSMIC palette not seeded"

echo "Seeding PHOSPHOR theme (COSMIC + GTK + icons + foot/btop/starship) into /etc/skel..."
# `kdos theme` is the single generator — running it against skel keeps the
# seeds byte-identical to what a live `kdos theme phosphor` produces. It is
# also what MATERIALIZES ~/.themes/KDOS and ~/.icons/KDOS: the packages only
# install the system copies plus their generators, because the appbox shares
# $HOME and not /usr/share, so the home copies are the ones alien apps see.
HOME=/etc/skel XDG_CONFIG_HOME=/etc/skel/.config XDG_CACHE_HOME=/etc/skel/.cache \
    /usr/local/bin/kdos theme phosphor
ls /etc/skel/.config/cosmic/com.system76.CosmicTheme.Dark/v2 2>/dev/null | head -3 || true
test -s /etc/skel/.config/gtk-3.0/gtk.css
test -s /etc/skel/.config/gtk-4.0/gtk.css
test -s /etc/skel/.themes/KDOS/gtk-3.0/gtk.css
test -s /etc/skel/.icons/KDOS/index.theme
