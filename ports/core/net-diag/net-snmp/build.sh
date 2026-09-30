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

# configure prompts for its defaults unless every prompted value is given
# here. The library, the tools (snmpget, snmpwalk, snmptrap...), the agent
# (snmpd, snmptrapd) and the MIB files are built; the Perl and Python
# bindings, embedded Perl and the Perl scripts (mib2c, snmpconf, tkmib) are
# off. USM crypto is OpenSSL; interface and route tables come from libnl.
# PCRE is PCRE1, which is not a port, so the process-table regex match is
# off; RPM, MySQL, libwrap, libelf and systemd are off.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--disable-static \
	--enable-shared \
	--with-defaults \
	--with-default-snmp-version=3 \
	--with-sys-contact=root@localhost \
	--with-sys-location=unknown \
	--with-logfile=/var/log/snmpd.log \
	--with-persistent-directory=/var/lib/net-snmp \
	--with-openssl \
	--enable-blumenthal-aes \
	--enable-ipv6 \
	--with-nl \
	--without-pcre \
	--without-rpm \
	--without-elf \
	--without-mysql \
	--without-libwrap \
	--without-systemd \
	--without-perl-modules \
	--disable-embedded-perl \
	--without-python-modules \
	--disable-scripts
grep -q '^#define HAVE_LIBCRYPTO 1' include/net-snmp/net-snmp-config.h
grep -q '^#define HAVE_LIBNL3 1' include/net-snmp/net-snmp-config.h
make
# The install rules race each other under a parallel make.
make -j1 DESTDIR=$PKG install

install -d -m 700 "$PKG/var/lib/net-snmp"

# snmpd IS SKIPPED UNTIL CONFIGURED. /etc/snmp/snmpd.conf does not exist until
# the administrator writes one, and an agent with no access lines answers
# nobody. It stays in the foreground under the supervisor and logs to syslog's
# daemon facility. snmptrapd has no script.
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/71_snmpd.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="snmpd"
DAEMON="/usr/sbin/snmpd"
CONF="/etc/snmp/snmpd.conf"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        [ -s "$CONF" ] || { echo "[SKIP] $NAME: no $CONF"; exit 0; }
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" -f -Lsd
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/71_snmpd.sh"
