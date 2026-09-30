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

# configure.ac appends -Werror to CPPFLAGS. CFLAGS and CXXFLAGS follow
# CPPFLAGS on every compile line, so -Wno-error there wins; without it a
# warning a newer compiler adds stops the build.
export CFLAGS="$CFLAGS -Wno-error"
export CXXFLAGS="$CXXFLAGS -Wno-error"

# The SMI-S plugin needs pywbem and ledmon backs LED control; neither is a
# port. The test suite needs check, chrpath and valgrind. The systemd unit,
# tmpfiles and sysusers files are not installed: the init script below makes
# the socket directory, and postinstall.sh makes the `libstoragemgmt`
# account lsmd drops to for plugins that do not need root.
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

# After 01_udev, whose device database the local plugin reads. lsmd exits
# when /run/lsm/ipc is missing or not writable, and udisks2's LSM module and
# lsmcli reach it only through the sockets it makes there; the modes and
# group are those of upstream's tmpfiles entry.
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
