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


# THE `systemd` MESON OPTION IS DEAD and must not be mistaken for the switch.
# 0.9.11 declares it `DEPRECATED` and reads it nowhere; the init-system branch
# is decided by `dependency('systemd', required: false)` at setup. With no
# systemd.pc on this image that probe fails, the unit file is not installed,
# and `install_emptydir` creates the device database directory the unit would
# otherwise have made. So the non-systemd path is what a plain setup already
# takes here, and passing -Dsystemd=false would change nothing while claiming
# to.
#
# NOTHING LINKS sd-bus EITHER. boltd talks D-Bus through GDBus, and its
# readiness and watchdog pings are its own `bolt_sd_notify_literal`, a write to
# $NOTIFY_SOCKET with no library behind it — unset here, so the calls are
# no-ops. basu is not a dependency and adding it would link nothing.
#
# --localstatedir=/var PINS THE DEVICE DATABASE AT /var/lib/boltd. meson
# derives localstatedir from the prefix and only /usr yields /var; the path is
# then frozen into the daemon as BOLT_DBDIR and substituted into the installed
# emptydir, so a prefix this recipe did not state would compile boltd to keep
# enrolled device keys somewhere else — /var/local/lib under /usr/local, and a
# path relative to the working directory under any other prefix.
#
# boltd IS INSTALLED STRAIGHT INTO libexecdir, with no subdirectory of its own,
# and the same string is substituted into the D-Bus service file's Exec=. The
# default /usr/libexec is therefore kept: overriding it to /usr/lib would drop
# a bare daemon among the shared libraries, and fprintd — the other
# D-Bus-activated system daemon here — already lives in /usr/libexec.
#
# -Dprivileged-group=wheel MATCHES /etc/group, and on this image it grants
# nothing. The installed polkit rule gives that group enroll, authorize and
# manage only when `subject.active && subject.local`, and with no session
# tracking here no subject is ever active, so every check falls to the
# policy's auth_admin with no agent to answer it: `boltctl enroll` and
# `authorize` are run under sudo. The flag keeps the rule naming a group that
# exists, so that it stays a rule for wheel rather than for nobody.
#
# -Dman=true MAKES THE MAN PAGES A HARD REQUIREMENT. The default `auto` builds
# them when a2x happens to be found, which silently drops boltctl(1) and
# boltd(8) whenever asciidoc is not installed first; asciidoc is in `depends`
# precisely so this cannot happen quietly.
#
# -Dinstall-tests=false KEEPS THE TEST PROGRAMS OUT OF THE PACKAGE. They are
# compiled regardless — bolt has no option to skip building them — and the
# umockdev-only ones are simply not in the list, because that dependency is
# `required: false` and umockdev is not on this image.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--localstatedir=/var \
	--buildtype=release \
	-Dman=true \
	-Dinstall-tests=false \
	-Dprivileged-group=wheel
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# NOTHING STARTS boltd BUT THE BUS. Upstream's 90-bolt.rules starts it through
# SYSTEMD_WANTS, which eudev ignores, so the daemon is only ever D-Bus
# activated — and an enrolled dock is authorised only while boltd runs. Two
# pokes cover the two moments a controller appears: the udev rule on every
# thunderbolt `add` once the bus is up, and the init script at boot, because
# 01_udev.sh's coldplug replays those `add`s before 40_dbus.sh has started the
# bus and the rule's dbus-send then has nothing to talk to. Both are a Ping to
# the name, which activates boltd when it is not running and is answered by it
# when it is, so neither can start a second daemon. A RUN= program cannot BE
# the daemon: udevd kills whatever a rule forks once the event is handled.
install -d "$PKG/usr/lib/udev/rules.d"
cat > "$PKG/usr/lib/udev/rules.d/91-kdos-bolt.rules" <<'RULES'
ACTION=="add", SUBSYSTEM=="thunderbolt", RUN+="/usr/bin/dbus-send --system --type=method_call --dest=org.freedesktop.bolt /org/freedesktop/bolt org.freedesktop.DBus.Peer.Ping"
RULES
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/43_boltd.sh" <<'KDOS_SH'
#!/bin/bash
NAME="boltd"

case "$1" in
    start)
        # No controller, nothing to authorise; the udev rule starts boltd if
        # one appears later.
        if [ -z "$(ls -A /sys/bus/thunderbolt/devices 2>/dev/null)" ]; then
            echo "[SKIP] $NAME: no thunderbolt controller"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        dbus-send --system --type=method_call \
            --dest=org.freedesktop.bolt /org/freedesktop/bolt \
            org.freedesktop.DBus.Peer.Ping
        ;;
    stop)
        # Started by the bus, so not ours to stop; it ends with the machine.
        ;;
    status)
        # Running is owning the bus name, which is also what a client needs.
        if dbus-send --system --print-reply --dest=org.freedesktop.DBus \
            / org.freedesktop.DBus.NameHasOwner string:org.freedesktop.bolt \
            2>/dev/null | grep -q 'boolean true'; then
            echo "$NAME is running"
        else
            echo "$NAME is not running"
            exit 1
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/43_boltd.sh"
