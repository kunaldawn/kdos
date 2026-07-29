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

if ! command -v kdos-theme-helper >/dev/null 2>&1; then
    echo "Warning: kdos-theme-helper not installed — skipping theme seed"
    exit 0
fi

echo "Seeding PHOSPHOR COSMIC theme into /etc/skel..."
XDG_CONFIG_HOME=/etc/skel/.config kdos-theme-helper \
    39ff14 000a03 04120a b8ffc8 ffb000 ff3131
ls /etc/skel/.config/cosmic/com.system76.CosmicTheme.Dark/v2 | head -3
