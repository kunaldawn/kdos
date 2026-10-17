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

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--disable-x \
	--disable-java-bindings \
	--disable-ocaml-bindings \
	--disable-tcl-bindings \
	--enable-python-bindings \
	--disable-lua-bindings \
	--disable-emacs-bindings \
	--disable-lisp-bindings \
	--enable-liblouis \
	--disable-gpm \
	--enable-i18n \
	--enable-icu \
	--enable-polkit \
	--enable-expat \
	--with-curses=ncurses \
	--with-rgx-package=libpcre2-32 \
	--with-pcm-package=alsa \
	--with-midi-package=alsa \
	--with-service-package=no \
	--without-libbraille \
	--without-espeak \
	--without-flite \
	--without-mikropuhe \
	--without-swift \
	--without-theta \
	ac_cv_header_eci_h=no \
	ac_cv_lib_intl_main=no \
	--with-screen-driver=lx,em,tx,-all \
	--with-speech-driver=eSpeak-NG
make
make install INSTALL_ROOT=$PKG CONFLIBDIR=:
compgen -G "$PKG/usr/lib/python3*/site-packages/brlapi*.so" >/dev/null || {
	echo 'brltty: the Python brlapi module was not built; Orca would have no braille' >&2
	exit 1
}

make install-polkit INSTALL_ROOT=$PKG

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/65_brltty.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="brltty"
DAEMON="/usr/bin/brltty"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # The configuration is the only opt-in. An attached serial or USB
        # device is no sign of a braille display — most are something else —
        # and nothing installs /etc/brltty.conf: a machine starts BRLTTY once
        # somebody writes one naming the display driver or speech path.
        if [ ! -s /etc/brltty.conf ]; then
            echo "[SKIP] $NAME: no /etc/brltty.conf"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # Speech plays through ALSA `default`, which is the session's
        # PipeWire, and init starts no PipeWire: without this the voice is
        # "Host is down" and silence. kdos_card is the card itself and is
        # exclusive — while a desktop session's PipeWire holds the card this
        # cannot open it, and while this speaks PipeWire cannot.
        export KDOS_ALSA_DEFAULT=kdos_card
        supervise "$NAME" "$DAEMON" -n
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/65_brltty.sh"
