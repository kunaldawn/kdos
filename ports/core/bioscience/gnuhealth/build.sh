# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

_home=/usr/lib/gnuhealth

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

_mods=
for d in tryton/health*; do
	case ${d##*/} in
	health_federation|health_genetics_uniprot|health_orthanc) ;;
	*) _mods="$_mods $d" ;;
	esac
done

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	defusedxml genshi passlib polib proteus puremagic pycountry \
	pydicom python-barcode python-sql python-stdnum pywebdav3-gnuhealth \
	relatorio simpleeval werkzeug vobject \
	trytond trytond-account trytond-account-invoice trytond-account-product \
	trytond-company trytond-country trytond-currency trytond-party \
	trytond-product trytond-stock trytond-stock-lot \
	gnuhealth-control $_mods

find "$PKG$_home" -path '*/trytond/modules/*/locale/*.po' -delete

install -d "$PKG$_home/libexec" "$PKG/usr/bin"
cat > "$PKG$_home/libexec/launch" <<'KDOS_SH'
#!/bin/sh
home=/usr/lib/gnuhealth
site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$home") || exit 1
plat=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("platlib", vars={"platbase": sys.argv[1]}))' "$home") || exit 1
PYTHONPATH=$site:$plat${PYTHONPATH:+:$PYTHONPATH}
TRYTOND_CONFIG=${TRYTOND_CONFIG:-/etc/gnuhealth/trytond.conf}
export PYTHONPATH TRYTOND_CONFIG
exec "$home/bin/${0##*/}" "$@"
KDOS_SH
chmod 755 "$PKG$_home/libexec/launch"
for p in trytond trytond-admin trytond-cron trytond-worker trytond-console \
	trytond-stat ghcontrol; do
	test -x "$PKG$_home/bin/$p"
	ln -s "../lib/gnuhealth/libexec/launch" "$PKG/usr/bin/$p"
done

install -dm750 "$PKG/var/lib/gnuhealth" "$PKG/var/lib/gnuhealth/attach"
install -Dm644 /dev/stdin "$PKG/etc/gnuhealth/trytond.conf" <<'CFG'
# GNU Health server (trytond 7.0).
#
# To bring up a clinic database, once PostgreSQL has a cluster and runs:
#
#   su - postgres -s /bin/sh -c 'createuser gnuhealth'
#   su - postgres -s /bin/sh -c 'createdb -O gnuhealth -E UTF8 health'
#   su gnuhealth -s /bin/sh -c 'trytond-admin -d health --all --email admin@localhost'
#   su gnuhealth -s /bin/sh -c 'trytond-admin -d health -u health --activate-dependencies'
#
# then uncomment the uri below and start the service. The Tryton client
# connects to <this machine>:8000, database "health".

[database]
# The role is the unix account the server runs as (peer authentication over
# the local socket), so the uri names no user and no password.
#uri = postgresql:///
path = /var/lib/gnuhealth/attach

[web]
listen = 0.0.0.0:8000
CFG

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/85_gnuhealth.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="gnuhealth"
DAEMON="/usr/bin/trytond"
CONF="/etc/gnuhealth/trytond.conf"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # The postinstall creates the account in /etc/passwd itself.
        if ! grep -q '^gnuhealth:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no gnuhealth account (the port's postinstall makes it)"
            exit 0
        fi
        if ! grep -q '^uri *=' "$CONF" 2>/dev/null; then
            echo "[SKIP] $NAME: no database uri in $CONF"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=gnuhealth --regid=gnuhealth \
            --init-groups "$DAEMON" -c "$CONF"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/85_gnuhealth.sh"
