# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE ONLY THING ON THIS MACHINE THAT CREATES COMMS RATHER THAN READING AN
# ARCHIVE. Everything else offline is a corpus somebody else wrote; a LAN XMPP
# server is two people on a stick talking to each other with no network beyond
# the room. dino-im already ships as a GUI client in the box.
#
# --lua-version 5.4 AND THE THREE PATHS THAT GO WITH IT. ports/core/lua is
# 5.5.0 and Prosody 13 supports 5.2 through 5.4, so this builds against the
# parallel lua54 port: the interpreter is `lua5.4`, its headers are in
# include/lua5.4 and its library is liblua5.4. Naming only the version would
# find the 5.5 headers on the default include path and compile against them.
./configure --prefix=/usr --sysconfdir=/etc/prosody \
	--datadir=/var/lib/prosody \
	--lua-version=$_lv \
	--with-lua-bin=/usr/bin \
	--with-lua-include=/usr/include/lua$_lv \
	--with-lua-lib=/usr/lib \
	--lua-suffix=$_lv \
	--idn-library=idn \
	--with-random=getrandom \
	--no-example-certs \
	--ostype=linux
make
make DESTDIR=$PKG install

# THE SERVER RUNS UNDER ksvc, AS ITS ACCOUNT. `prosody` refuses to start as
# root and never drops privileges itself (only prosodyctl does), so setpriv
# makes it the postinstall's account before the exec. -F keeps it in the
# foreground, where the supervisor can watch it.
#
# IT SKIPS UNTIL AN ACCOUNT EXISTS. The shipped configuration serves
# `localhost` with registration off, so a server with no account has nobody
# it can let in — and every machine carrying the port would otherwise hold
# 5222 and 5269 open behind the firewall for nothing.
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/74_prosody.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="prosody"
DAEMON="/usr/bin/prosody"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^prosody:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no prosody account (the port's postinstall makes it)"
            exit 0
        fi
        # Accounts live at <data>/<host>/accounts/<user>.dat; the first is
        # made with `prosodyctl adduser <user>@<host>`.
        if ! compgen -G '/var/lib/prosody/*/accounts/*.dat' >/dev/null; then
            echo "[SKIP] $NAME: no account yet (prosodyctl adduser makes one)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=prosody --regid=prosody \
            --init-groups "$DAEMON" -F
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/74_prosody.sh"
