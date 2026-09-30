# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The source is GNU Health's own repository at the release tag: every health
# module in one tree, under tryton/. The Tryton server, the Tryton modules
# GNU Health builds on and the rest of the closure that no other port carries
# are in the bundle; everything shared (lxml, pillow, matplotlib, dateutil,
# pytz, qrcode, psutil, psycopg2) is a port found in site-packages.
#
# THE SERVER IS 7.0, the long-term series GNU Health 5.0 is written for; the
# `tryton` client port is the same series.
#
# THE CLOSURE LIVES UNDER ITS OWN PREFIX, /usr/lib/gnuhealth. vobject,
# defusedxml and polib are also vendored into site-packages or a private
# prefix by other ports, and under this one nothing else can own them.
# vobject is 0.9.8: 0.9.9's metadata imports dateutil, which the fetch's
# isolated build does not have, and the caldav module runs on either.
#
# Three modules are not installed, because each exists to reach a service
# this system does not have: health_federation (a Thalamus server),
# health_genetics_uniprot (UniProt on the internet) and health_orthanc (an
# Orthanc PACS, through pyorthanc, which is not in the bundle).
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

# hatch-tryton is the build plugin of python-sql and relatorio. It goes into
# the build root, not into $PKG; --break-system-packages because python3
# marks its site-packages as kpkg's (PEP 668).
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--break-system-packages hatch-tryton

# --ignore-installed: a module that is also in site-packages still goes under
# the prefix, where the launchers look first.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	defusedxml genshi passlib polib proteus puremagic pycountry \
	pydicom python-barcode python-sql python-stdnum pywebdav3-gnuhealth \
	relatorio simpleeval werkzeug vobject \
	trytond trytond-account trytond-account-invoice trytond-account-product \
	trytond-company trytond-country trytond-currency trytond-party \
	trytond-product trytond-stock trytond-stock-lot \
	gnuhealth-control $_mods

# English only. A module's locale/<lang>.po is loaded into the database only
# for a language an administrator makes translatable; English is the source
# text and has no file.
find "$PKG$_home" -path '*/trytond/modules/*/locale/*.po' -delete

# EVERY COMMAND RUNS THROUGH ONE LAUNCHER, which puts the prefix's
# site-packages in front of the system's and names the configuration file,
# then runs the program of its own name from the prefix.
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

# THE SHIPPED CONFIGURATION SERVES THE LAN once a database exists. The uri is
# commented out, and the service skips until it is set, so a machine that
# carries the port holds no port open for nothing.
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

# THE SERVER RUNS UNDER ksvc, AS ITS ACCOUNT, and it skips until the uri is
# set: without a database it has nothing to serve.
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
        # Read /etc/passwd directly: there is no getent on musl.
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
