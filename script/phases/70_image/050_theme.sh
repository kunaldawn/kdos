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
# Seed the DEFAULT accent into /etc/skel so a fresh home (live ISO, new user)
# logs in to the KDOS look. Runs before 070_user.sh materializes homes (the
# step number does the sequencing). The Plasma identity kit itself — colour
# schemes, global themes, Konsole and qt5ct palettes — is installed by the
# packages under /usr/share and needs nothing here; what a home carries is
# the accent's NAME and the files of the programs that read only a home:
# qt5ct, btop, starship and the other terminal programs.

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

echo "Seeding the $KDOS_ACCENT accent (Plasma, Konsole, qt5ct, terminal programs) into /etc/skel..."
# `kdos theme` is the single writer — running it against skel keeps the seeds
# byte-identical to what `kdos theme <accent>` writes outside a session: the
# accent file, the global theme the first login applies, and the home files
# of the programs that cannot read /usr/share/kdos/xdg.
HOME=/etc/skel XDG_CONFIG_HOME=/etc/skel/.config XDG_CACHE_HOME=/etc/skel/.cache \
    XDG_DATA_HOME=/etc/skel/.local/share \
    /usr/local/bin/kdos theme "$KDOS_ACCENT"
test "$(cat /etc/skel/.config/kdos/accent)" = "$KDOS_ACCENT"
# Read through the cascade, as startplasma does at the first login, and not out
# of the file: KConfig writes no key whose value the machine's /etc/xdg/kdeglobals
# already gives, and that absence is what lets the installer's accent reach a
# new account.
_lnf=$(XDG_CONFIG_HOME=/etc/skel/.config kreadconfig6 --file kdeglobals --group KDE --key LookAndFeelPackage)
if [ "$_lnf" != "org.kdos.$KDOS_ACCENT.desktop" ]; then
    echo "FATAL: a new home's global theme is '$_lnf', not org.kdos.$KDOS_ACCENT.desktop" >&2
    exit 1
fi
# The Qt 5 applications have no Plasma platform theme, so qt5ct is their only
# way to the accent: its configuration must name a palette the image carries,
# or every Qt 5 application paints qt5ct's own defaults.
_qt5=$(sed -n 's/^color_scheme_path=//p' /etc/skel/.config/qt5ct/qt5ct.conf)
case "$_qt5" in
/usr/share/qt5ct/colors/KDOS-*.conf) test -s "$_qt5" ;;
*) echo "FATAL: skel's qt5ct.conf names no KDOS palette: '$_qt5'" >&2; exit 1 ;;
esac
# Every installed KDE colour scheme clears the contrast floors. The schemes
# are generated at build time from libkcolor's table; a palette edit that
# drops one role under 4.5:1 stops the image here rather than on a screen.
compgen -G '/usr/share/color-schemes/KDOS*.colors' >/dev/null
kdos-theme contrast -q /usr/share/color-schemes/KDOS*.colors

# THE LOGIN SCREEN WEARS THE ACCENT FROM THE FIRST BOOT. The greeter reads
# its look from the sddm account's home: ~/.config/kdeglobals, plasmarc and
# kcminputrc, and ~/kdos-background.png, the picture the KDOS greeter theme
# shows. kdos-powerd and the installer lay that set down when the accent is
# chosen; nothing chooses one on the live medium, so the image carries the
# default accent's set. All or nothing, as kdos-powerd's copy is: a missing
# file stops the image rather than shipping a greeter half in one accent. The
# home is handed to the account, which owns ~/.config because the greeter's
# KWin writes kwinoutputconfig.json there; a recursive chown is safe in the
# build root, where no account has run to leave a link.
echo "Seeding the $KDOS_ACCENT login screen into the sddm account's home..."
_gset=/usr/share/kdos/greeter/$KDOS_ACCENT
_ghome=$(awk -F: '$1 == "sddm" { print $6; exit }' /etc/passwd)
case "$_ghome" in
/var/lib/?*) ;;
*) echo "FATAL: the sddm account's home is '$_ghome', not under /var/lib" >&2; exit 1 ;;
esac
for _f in kdeglobals plasmarc kcminputrc background.png; do
    test -s "$_gset/$_f" || { echo "FATAL: the $KDOS_ACCENT greeter set has no $_f" >&2; exit 1; }
done
install -d -m 0750 "$_ghome"
install -d -m 0755 "$_ghome/.config"
for _f in kdeglobals plasmarc kcminputrc; do
    install -m 0644 "$_gset/$_f" "$_ghome/.config/$_f"
done
install -m 0644 "$_gset/background.png" "$_ghome/kdos-background.png"
chown -R sddm:sddm "$_ghome"

# The machine's accent record names the accent the splash and the login screen
# now wear. Without it the Look page and the About page cannot say what the
# screens before login wear, and they report Plasma and the terminals alone.
# No newline, as kdos-powerd and the installer write it.
install -d -m 0755 /etc/kdos
printf '%s' "$KDOS_ACCENT" > /etc/kdos/accent
chmod 0644 /etc/kdos/accent

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
# and Plasma's Open With only through
# /usr/share/applications/mimeinfo.cache. kpkg's desktop trigger
# writes it when a package touches the directory, which an fs/-only rebuild
# never does, so it is rebuilt here too.
if command -v update-desktop-database >/dev/null 2>&1 &&
   [ -d /usr/share/applications ]; then
    update-desktop-database -q /usr/share/applications
fi
