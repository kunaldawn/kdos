#!/bin/bash
. /etc/init.d/service_helper

# org.freedesktop.login1, FROM elogind, SUPERVISED, AND NEVER STARTED BY THE BUS.
#
# Every login registers here through pam_elogind: the SDDM greeter, each
# Plasma session, a tty2 login, and the user services turnstile runs. KWin
# takes the seat through it (TakeControl, the DRM and input devices), polkit
# asks it whether a caller is in the active local session, and Plasma's Shut
# Down, Restart and Suspend are its PowerOff, Reboot and Suspend calls. It is
# not a supervisor: PID 1 is still sysvinit, a Shut Down still ends in
# runlevel 0 and rcK, and the user services are turnstile's and ksvc's.
#
# AFTER dbus (40_dbus sorts first) AND BEFORE polkitd (41), NetworkManager
# (42) and everything that asks about sessions: `wait_bus_name` holds the boot
# here until the name is owned. The elogind port ships no activation file, so
# a call that came first would be refused rather than start a second,
# unsupervised elogind that then holds the name the supervised one needs.
#
# --restart-with dbus, because a restarted system bus leaves elogind holding a
# name on a bus that is gone. A restarted elogind still knows every session —
# it keeps their state under /run/systemd and reads it back — but not a
# compositor's hold on its seat: the DRM and input descriptors it handed KWin
# would survive only in a service manager's file store, and ksvc keeps none,
# so a desktop running across an elogind restart is best logged out of.
#
# Its configuration is /etc/elogind/logind.conf.d/50-kdos.conf and
# /etc/elogind/sleep.conf.d/50-kdos.conf; the runtime directories under
# /run/user are turnstile's, not elogind's.

NAME="elogind"
DAEMON="/usr/lib/elogind/elogind"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # In the foreground (no --daemon), so the supervisor watches the
        # daemon itself; its comm is then `elogind`, the name kdos-oomd
        # protects.
        #
        # SYSTEMD_REBOOT_TO_BOOT_LOADER_ENTRY=1 makes login1's
        # RebootToBootLoaderEntry the file `kdos-bootctl try` writes,
        # /run/systemd/reboot-to-boot-loader-entry. Without it elogind reads
        # only the EFI LoaderEntryOneShot variable, which Limine never sets,
        # so the logout screen cannot name the slot a restart boots.
        export SYSTEMD_REBOOT_TO_BOOT_LOADER_ENTRY=1
        supervise --restart-with dbus "$NAME" "$DAEMON"
        wait_bus_name org.freedesktop.login1
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
