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

./configure --prefix=/usr --sysconfdir=/etc/nut --libexecdir=/usr/lib/nut \
	--datadir=/usr/share/nut \
	--disable-static \
	--with-user=nut --with-group=nut \
	--with-statepath=/run/nut --with-altpidpath=/run/nut \
	--with-drvpath=/usr/lib/nut \
	--with-udev-dir=/usr/lib/udev \
	--with-serial --with-usb --with-modbus --with-gpio --with-linux_i2c \
	--with-snmp --with-neon --with-ipmi --with-freeipmi \
	--with-powerman --without-upower --without-macosx_ups \
	--with-avahi --with-libltdl --with-nut-scanner \
	--with-ssl --with-openssl --without-nss \
	--without-wrap --without-cgi \
	--without-nut_monitor --without-pynut --without-nutconf \
	--with-dev \
	--with-doc=man=dist-auto \
	--with-systemdsystemunitdir=no --with-systemdsystempresetdir=no \
	--with-systemdshutdowndir=no --with-systemdtmpfilesdir=no \
	--with-systemdsysusersdir=no --without-libsystemd \
	--with-hotplug-dir=no --with-devd-dir=no \
	--with-augeas-lenses-dir=no \
	--with-solaris-smf=no --with-solaris-init=no \
	--with-solaris-pkg-svr4=no --with-solaris-pkg-ips=no \
	--enable-cppunit=no
make
make DESTDIR=$PKG install

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/56_nut.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

CONF="/etc/nut/nut.conf"

# The MODE line of nut.conf, unquoted, without a trailing comment.
nut_mode() {
    [ -r "$CONF" ] || return 0
    while IFS= read -r l; do
        case "$l" in
            MODE=*) l=${l#MODE=}; l=${l//\"/}; echo "${l%%[[:space:]#]*}"; return ;;
        esac
    done < "$CONF"
}

case "$1" in
    start)
        [ ! -x /usr/sbin/upsd ] && { echo "[SKIP] nut: /usr/sbin/upsd not found"; exit 0; }
        mode=$(nut_mode)
        case "$mode" in
            standalone|netserver|netclient) ;;
            *) echo "[SKIP] nut: MODE is '${mode:-none}' in $CONF"; exit 0 ;;
        esac
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^nut:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] nut: no nut account (the port's postinstall makes it)"
            exit 0
        fi
        install -d -o nut -g nut -m 770 /run/nut
        echo "[KDOS] Starting nut ($mode)..."
        if [ "$mode" != netclient ]; then
            supervise nut-drivers /usr/sbin/upsdrvctl -F start
            supervise nut-upsd /usr/sbin/upsd -F
        fi
        supervise nut-upsmon /usr/sbin/upsmon -F
        ;;
    stop)
        stop_service nut-upsmon
        stop_service nut-upsd
        stop_service nut-drivers
        ;;
    status)
        check_status nut-upsmon
        ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/56_nut.sh"
