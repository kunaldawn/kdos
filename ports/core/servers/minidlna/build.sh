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


# Alpine's two patches: ffmpeg7 reads the channel count from the channel
# layout, the only place FFmpeg 7 and later keep it, and libav-fix takes
# libavutil off the test program's link line, where the configure script
# defines no variable for it. The second edits Makefile.am, so the build
# system is regenerated.
patch -p1 -i "$PORT_SRC/ffmpeg7.patch"
patch -p1 -i "$PORT_SRC/libav-fix.patch"
autoreconf -fi

# -fcommon: the sources define the same globals in several files, which GCC
# links only as common symbols. NLS is off: the catalogues are translations
# of the log and the web status page, and this image carries English only.
# Avahi is compiled in only with TiVo support, which is off, so it is not a
# dependency; configure still probes for it unconditionally and would link
# libavahi-client whenever avahi happens to be in the build root, so the
# probe's answer is given as no.
export CFLAGS="$CFLAGS -fcommon"
./configure --prefix=/usr --sysconfdir=/etc \
	ac_cv_lib_avahi_client_avahi_threaded_poll_new=no \
	--with-db-path=/var/lib/minidlna \
	--with-log-path=/var/log/minidlna \
	--with-os-name=KDOS --with-os-url=https://github.com/kunaldawn/kdos \
	--disable-nls --disable-tivo --disable-netgear --disable-readynas
make
make DESTDIR=$PKG install

install -Dm644 minidlna.conf.5 "$PKG/usr/share/man/man5/minidlna.conf.5"
install -Dm644 minidlnad.8 "$PKG/usr/share/man/man8/minidlnad.8"
install -Dm644 minidlna.conf "$PKG/usr/share/doc/minidlna/minidlna.conf.example"
install -dm755 "$PKG/var/lib/minidlna" "$PKG/var/log/minidlna"

# THE SERVER RUNS UNDER ksvc ONCE /etc/minidlna.conf EXISTS. What to share
# is the configuration's media_dir lines, and there is no default worth
# announcing to every television on the network; the template is in
# /usr/share/doc/minidlna. -S keeps minidlnad in the foreground, and -u
# drops it to the postinstall's account once its sockets are open. SSDP
# (1900/udp) and the HTTP port (8200) stay closed to other machines until
# the firewall's rule for them is on.
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/84_minidlna.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="minidlna"
DAEMON="/usr/sbin/minidlnad"
CONF="/etc/minidlna.conf"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ ! -s "$CONF" ]; then
            echo "[SKIP] $NAME: no $CONF (/usr/share/doc/minidlna has the template)"
            exit 0
        fi
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^minidlna:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no minidlna account (the port's postinstall makes it)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" -S -u minidlna -f "$CONF"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/84_minidlna.sh"
