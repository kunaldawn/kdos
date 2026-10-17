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

export CFLAGS="$CFLAGS -Wno-error"
export CXXFLAGS="$CXXFLAGS -Wno-error"

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--disable-static \
	--without-test \
	--without-smispy \
	--without-ledmon \
	--with-bash-completion-dir=/usr/share/bash-completion/completions \
	--with-systemdsystemunitdir=no \
	--with-systemd-sysusersdir=no \
	--with-systemd-tmpfilesdir=no \
	PYTHON=python3
make
make DESTDIR=$PKG install

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/51_lsmd.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="lsmd"
DAEMON="/usr/bin/lsmd"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        echo "[KDOS] Starting $NAME..."
        install -d -m 775 -g libstoragemgmt /run/lsm /run/lsm/ipc
        # -d: stay in the foreground, so ksvc watches the daemon itself.
        supervise "$NAME" "$DAEMON" -d
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/51_lsmd.sh"
