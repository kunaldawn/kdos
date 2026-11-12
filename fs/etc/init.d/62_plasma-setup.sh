#!/bin/bash
# THE FIRST-BOOT WIZARD, ARMED ON A MACHINE INSTALLED FOR SOMEBODY ELSE.
#
# An OEM install (kinstall's `oem = 1`) makes no account and leaves
# /etc/plasma-setup-done absent; every other install writes it. While it is
# absent this step prepares plasma-setup's account and its home, then runs
# plasma-setup's own boot utility, which writes
# /etc/sddm.conf.d/99-plasma-setup.conf — SDDM logs the plasma-setup account
# in, and the wizard starts from that session's autostart — and takes the
# [Autologin] group out of kde_settings.conf. The wizard makes the first real
# account, writes /etc/plasma-setup-done and removes its autologin file, so
# the next boot shows the login screen and this step does nothing.
#
# BEFORE SDDM: inittab's tty1 line, which starts SDDM, is reached only once rcS
# has returned. After 40_dbus, so the boot utility's question to the system
# bus — which display manager runs — has a bus to answer it.
#
# What upstream ships as a sysusers.d and a tmpfiles.d file is done here,
# because nothing on this system reads either: the account is made once,
# with useradd, and the home is rebuilt at every boot it is needed, because
# /run is a tmpfs.
#
#   d   /run/plasma-setup                          the home
#   C   .config/autostart/plasma-setup.desktop     copied when absent
#   f   plasma-welcomerc, kscreenlockerrc,         written when absent: no
#       kwalletrc, the applets file                welcome tour, no screen
#                                                  lock and no wallet for an
#                                                  account with no password,
#                                                  and an empty, invisible
#                                                  panel
#   C+  .config/kglobalaccelrc                     copied over, every boot
#   Z   0750, the account's, all of it
#
# The live medium never runs it: its account is the live one, and the
# installer is what runs there.

NAME="plasma-setup"
DONE=/etc/plasma-setup-done
HOME_DIR=/run/plasma-setup
SHARE=/usr/share/plasma-setup
AUTOLOGIN=/etc/sddm.conf.d/99-plasma-setup.conf

live() {
    while read -r _dev _mnt _type _rest; do
        if [ "$_mnt" = / ] && [ "$_type" = overlay ]; then
            return 0
        fi
    done < /proc/mounts
    return 1
}

bootutil() {
    for _b in /usr/lib/libexec/plasma-setup-bootutil /usr/libexec/plasma-setup-bootutil; do
        [ -x "$_b" ] && { echo "$_b"; return 0; }
    done
    return 1
}

# "f": the file with this content, only when it is not there.
put() {
    [ -e "$1" ] || printf '%b' "$2" > "$1"
}

case "$1" in
    start)
        # A wizard that finished but did not get to remove its autologin
        # file would log the plasma-setup account in at every boot; the
        # marker is what says it finished.
        if [ -e "$DONE" ]; then
            rm -f "$AUTOLOGIN"
            exit 0
        fi
        if live; then
            echo "[SKIP] $NAME: the live medium sets nobody up"
            exit 0
        fi
        if ! _util=$(bootutil) || [ ! -f "$SHARE/plasma-setup.desktop" ]; then
            echo "[SKIP] $NAME: plasma-setup is not installed; the login screen is shown"
            exit 0
        fi
        echo "[KDOS] Preparing the first-boot setup..."
        if ! grep -q '^plasma-setup:' /etc/passwd; then
            if ! useradd --system --no-create-home --home-dir "$HOME_DIR" \
                    --shell /bin/sh --comment "Plasma Setup" --user-group \
                    plasma-setup; then
                echo "[FAIL] $NAME: cannot make the plasma-setup account"
                exit 1
            fi
        fi
        mkdir -p "$HOME_DIR/.config/autostart"
        [ -e "$HOME_DIR/.config/autostart/plasma-setup.desktop" ] ||
            cp "$SHARE/plasma-setup.desktop" "$HOME_DIR/.config/autostart/plasma-setup.desktop"
        put "$HOME_DIR/.config/plasma-welcomerc" \
            '[General]\nLastSeenVersion=6.4.2\nShowUpdatePage=false\n'
        put "$HOME_DIR/.config/kscreenlockerrc" \
            '[Daemon]\nAutolock=false\nLockOnResume=false\nTimeout=0\n'
        put "$HOME_DIR/.config/kwalletrc" '[Wallet]\nEnabled=false\n'
        put "$HOME_DIR/.config/plasma-org.kde.plasma.desktop-appletsrc" \
            '[Containments][1]\nformfactor=2\nlocation=3\nplugin=org.kde.panel\n\n[Containments][1][General]\nthickness=0\n'
        [ -f "$SHARE/kglobalaccelrc" ] &&
            cp "$SHARE/kglobalaccelrc" "$HOME_DIR/.config/kglobalaccelrc"
        chown -R plasma-setup:plasma-setup "$HOME_DIR"
        chmod 0750 "$HOME_DIR"
        if ! "$_util"; then
            echo "[FAIL] $NAME: plasma-setup-bootutil did not arm the login"
            exit 1
        fi
        echo "[KDOS] The first start runs the setup wizard"
        ;;
    stop)
        # One-shot — nothing to stop
        ;;
    status)
        # The autologin file is what arms the wizard, so it, not the absent
        # marker, is what "armed" is read from.
        if [ -e "$DONE" ]; then
            echo "[ OK ] $NAME: set up"
        elif live; then
            echo "[SKIP] $NAME: the live medium sets nobody up"
        elif [ -e "$AUTOLOGIN" ]; then
            echo "[ OK ] $NAME: armed for the next login"
        else
            echo "[DOWN] $NAME: not armed; the login screen is shown"
            exit 1
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
