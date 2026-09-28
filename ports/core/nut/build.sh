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


# Every driver family is named, because configure's `auto` answers a missing
# library by leaving the drivers out. Built: serial, USB (libusb), Modbus
# (libmodbus, for solar charge controllers and inverters), GPIO (libgpiod),
# I2C (i2c-tools' libi2c), SNMP (net-snmp), the XML/HTTP driver (neon), IPMI
# power supplies (FreeIPMI) and PDUs through a powerman daemon (libpowerman).
# Not built: the UPower driver. nut-scanner loads its backends through
# libltdl, and finds NUT servers on the LAN through Avahi. TLS is OpenSSL;
# NSS is off. The CGI pages, the python GUI, PyNUT and every systemd,
# Solaris and hotplug file are off. The manual pages are the ones the
# release tarball carries.
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

# THE SAMPLES STAY SAMPLES. /etc/nut/*.sample are upstream's templates;
# nut.conf's MODE line is what starts anything below, and with no nut.conf
# nothing runs. upsmon.conf.sample's SHUTDOWNCMD names /sbin/shutdown, which
# this system does not have: `poweroff` is the command to write there.
#
# THE SERVICES RUN UNDER ksvc, IN THE FOREGROUND (-F). MODE=standalone or
# netserver starts the drivers (through upsdrvctl), upsd and upsmon;
# netclient starts upsmon alone, watching another machine's upsd. Each
# program started as root drops to the nut account itself, except upsmon's
# parent, which stays root to run the shutdown. /run/nut is made here
# because the drivers and upsd, running as nut, cannot create it under
# /run. upsd listens on 3493, closed to other machines until the firewall's
# rule for it is on.
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
