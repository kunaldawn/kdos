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


# The makefile assigns CFLAGS itself, from CDEBUGFLAGS and the defines, so an
# exported CFLAGS is overridden; CDEBUGFLAGS is the variable meant to carry
# the compiler flags, and it takes the phase's.
make CDEBUGFLAGS="$CFLAGS -Wall"
make PREFIX=/usr TARGET=$PKG install

# THE DAEMON RUNS UNDER ksvc ONCE /etc/babeld.conf EXISTS. Which interfaces
# form the mesh is the configuration's `interface` lines, and there is no
# default: babeld on every interface would route a home LAN into a mesh
# nobody asked for. It stays in the foreground unless given -D, and runs as
# root because it writes the kernel's routing table. The Babel port,
# 6696/udp, is closed until the firewall's rule for it is on.
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/31_babeld.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="babeld"
DAEMON="/usr/bin/babeld"
CONF="/etc/babeld.conf"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ ! -s "$CONF" ]; then
            echo "[SKIP] $NAME: no $CONF"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" -c "$CONF" -S /var/lib/babeld/state
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/31_babeld.sh"
install -dm755 "$PKG/var/lib/babeld"
