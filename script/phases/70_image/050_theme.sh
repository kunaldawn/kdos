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
# boots straight into the KDOS look. Runs before 070_user.sh materializes homes
# (the step number does the sequencing). The palette itself is libkcolor's
# — kdos-comp and kdos-shell link it and read only the accent NAME from
# $XDG_CACHE_HOME/kdos/theme, so nothing here writes colours for the desktop.
# Everything below is for software that is not ours: GTK, foot, btop, starship.

set -e
source script/phases/70_image/phase.env

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
# The window frames, generated from the same palette as everything else so an
# accent switch reaches them; kdos-comp re-reads the file on the SIGHUP
# `kdos theme` sends.
test -s /etc/skel/.config/kdos-comp/themerc-override
# The cursors are generated here too, from the art kdos-cursors ships in
# /usr/share/kdos/cursors/art. The package installs a build of its own
# into /etc/skel/.icons; this rewrites it from the same generator and the same
# art, so the two agree by construction rather than by luck.
test -s /etc/skel/.icons/KDOS-cursors/cursors/default
# The KDE bridge: boxed dolphin/okular/kate read this file out of the shared
# home, and a skel without it hands every new user a grey KDE.
test -s /etc/skel/.config/kdeglobals
test -s /etc/skel/.local/share/color-schemes/KDOS.colors
# The Qt side for a process with no KDE platform theme: every Qt 5 application,
# and a Qt 6 one started under qt6ct. Each file's palette is the colour file
# beside it, named with a `~` so it resolves in the reading user's home.
test -s /etc/skel/.config/qt5ct/qt5ct.conf
test -s /etc/skel/.config/qt5ct/colors/KDOS.conf
test -s /etc/skel/.config/qt6ct/qt6ct.conf
test -s /etc/skel/.config/qt6ct/colors/KDOS.conf

# The GSettings defaults (fs/usr/share/glib-2.0/schemas/90_kdos.gschema.override)
# reach GTK only through gschemas.compiled. kpkg's schemas trigger writes it
# when a package touches the directory, which an fs/-only rebuild never does,
# so it is rebuilt here from whatever the directory holds now. No glib means
# no GSettings reader, and no schema means nothing for the override to set:
# glib-compile-schemas then writes no index at all, and the check below would
# stop the image build.
if command -v glib-compile-schemas >/dev/null 2>&1 &&
   compgen -G '/usr/share/glib-2.0/schemas/*.gschema.xml' >/dev/null; then
    glib-compile-schemas /usr/share/glib-2.0/schemas
    test -s /usr/share/glib-2.0/schemas/gschemas.compiled
fi

# The desktop entries fs/ ships (the kdos-* handlers) reach `kdos-appbox open`
# and the shell's Open With only through
# /usr/share/applications/mimeinfo.cache. kpkg's desktop trigger
# writes it when a package touches the directory, which an fs/-only rebuild
# never does, so it is rebuilt here too.
if command -v update-desktop-database >/dev/null 2>&1 &&
   [ -d /usr/share/applications ]; then
    update-desktop-database -q /usr/share/applications
fi
