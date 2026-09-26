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

# THE RELEASE TARBALL, NOT THE TAG ARCHIVE, and it saves two failures rather
# than one. brltty.app ships a pre-generated `configure`; the github archive
# does not, so it needs `./autogen`, and autogen brings both of these:
#
#   - it runs Tools/gendeps, a Tcl script that OPENS every source and follows
#     its includes. Under the phase env's LC_ALL=C, Tcl decodes as ASCII and
#     any byte over 127 is `error reading "file5": invalid or incomplete
#     multibyte or wide character` — from a file it never names, in sources
#     that are valid UTF-8 and merely have accented comments.
#   - it regenerates configure with THIS autoconf, which emits `\\(` where
#     upstream's emitted `\(` inside m4/brltty.m4's expr calls. The driver
#     lookup then fails on `expr: syntax error` and reports
#     `unknown speech driver:` with an empty name.
#
# Upstream's own configure has neither problem. `expr length` is still a GNU
# extension toybox lacks, which is why coreutils is a dependency.

# WHAT THIS READS BY DEFAULT IS /dev/vcsa, so it covers tty1 and the
# installer — both grids of cells it takes verbatim, with none of the guessing
# a screen reader does over a toolkit's accessibility tree. It does not reach
# the graphical session, which is Wayland and publishes no accessibility tree
# at all.
#
# THE DRIVER NAME IS `eSpeak-NG`, spelled exactly as the directory under
# Drivers/Speech — configure matches it case-sensitively and answers anything
# else with `unknown speech driver`, which reads like a missing library rather
# than a misspelling. The list takes NAMES, `all` or `-all` and nothing else;
# there is no `+`.
#
# AND A TRAILING `+` BREAKS THE PARSER RATHER THAN BEING REJECTED, because
# brltty splits the list with `expr "$items" : '[^,]*,'` and GNU expr reads a
# bare `+` as its own operator — the one that forces the next token to be a
# string. What comes out is `expr: syntax error: unexpected argument` and then
# `unknown speech driver:` with an EMPTY name, which points at the driver
# rather than at the separator. Neither expr can run this configure unaided:
# toybox's lacks `length`, which is why coreutils is a dependency, and GNU's
# eats the `+`.
#
# `,-all` ON THE SCREEN LIST, and without it the rest are built as EXTERNAL
# loadable drivers rather than skipped: the FileViewer screen driver links
# `-ltinfo`, which this ncurses does not build as a separate library. Naming a
# driver selects what is INTERNAL; only `-all` says what is not built at all.
# SCREENS: lx reads /dev/vcsa, em (TerminalEmulator) reads the pty brltty-pty
# runs, tx reads a tmux session.
#
# THE SPEECH LIST HAS NO `-all`, because SpeechDispatcher only works EXTERNAL:
# built in, its libspeechd never reaches brltty's own link line and the link
# fails on every spd_* symbol, while the module links it itself. So eSpeak-NG
# is named (internal) and every other registered speech driver is built as a
# module — SpeechDispatcher, which shares the system speech server, and the
# library-free ones (ExternalSpeech, GenericSay, Festival's pipe, and the three
# that speak through a braille display). What keeps a module out is that
# configure never registers it: --without-espeak for the OLD eSpeak driver,
# which links `-lespeak` and finds espeak-ng's compatibility header, the other
# vendor engines' --without-<engine>, and ac_cv_header_eci_h=no for ViaVoice,
# which has no switch and is registered whenever an eci.h exists.
#
# THE BRAILLE LIST IS LEFT AT ITS DEFAULT, which builds every driver configure
# registered as an external module under /usr/lib/brltty and loads the one
# /etc/brltty.conf names. Almost all of them — Baum, HandyTech, HumanWare,
# FreedomScientific, Papenmeier, Alva and the rest — speak to the display over
# brltty's own serial, USB, HID and Bluetooth I/O with no library behind them.
# Two are registered only when something is found: Libbraille, which
# --without-libbraille keeps out (not a port), and XWindow, which --disable-x
# keeps out. Naming either in the list would be an error rather than an
# exclusion, because a driver configure never registered is `unknown`.
#
# EVERY PROBE IS PINNED. Each package option below is a first-found list or
# an `if found` test, so a library that is not in `depends` changes the daemon
# with build order: bluez (Bluetooth displays, and HID over Bluetooth), dbus
# (the Bluetooth path's device lookup), polkit (BrlAPI authorisation for a
# session user), icu, expat (CLDR tables), libcap (dropping root
# capabilities), pcre2's 32-bit library (--with-rgx-package), gettext
# (translated messages, whose msgmerge --enable-i18n needs) and alsa-lib
# (tunes over PCM and MIDI). ac_cv_lib_intl_main=no keeps the i18n probe from
# linking the `libintl` port whenever it happens to be installed: musl's own
# gettext carries the translations. No
# --with-service-package: its only candidate is libsystemd, and `no` keeps the
# daemon a plain forking one. --with-curses=ncurses because brltty's
# `ncursesw` choice includes <ncursesw/ncurses.h> and this ncurses installs
# its wide headers straight into /usr/include; the TTY braille driver and
# brltty-pty are what use it. --enable-liblouis makes a contraction table
# named `louis:<file>` — `-c louis:en-ueb-g2.ctb` — a liblouis table, which is
# where grade 2 and every other contracted braille comes from; without it
# only brltty's own few contraction tables exist. GPM is not on this image.
#
# espeak-ng is what turns that into speech. STATED LIMIT: boxed GUI
# applications remain unreachable — they have no cells and there is no at-spi
# registry on this host. docs/kdos/02-user-guide/accessibility.md is the
# statement of record.
#
# --disable-x: the hard rule, and BRLTTY's X support exists to read an X screen
# this system does not have. The API server stays on because it is how
# anything else on the machine asks BRLTTY to speak. The Java, OCaml, Tcl,
# Python, Lua, Emacs and Lisp bindings are off: nothing here consumes them.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--disable-x \
	--disable-java-bindings \
	--disable-ocaml-bindings \
	--disable-tcl-bindings \
	--disable-python-bindings \
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
# INSTALL_ROOT, NOT DESTDIR. brltty's Makefiles use their own variable name,
# and DESTDIR is silently ignored — the install then writes into the LIVE
# chroot: `/bin/install -c -m 644 brlapi_constants.h /usr/include`, outside
# $PKG and outside the package's manifest. It fails eventually on ldconfig,
# which is the only reason anyone notices.
#
# CONFLIBDIR=: because musl HAS NO ldconfig. configure hardcodes
# `/sbin/ldconfig -n` for every linux host without looking for it, and the
# install then dies with `No such file or directory` AFTER every file is
# already in place. `:` is the value configure itself uses on a platform where
# the command does not exist; musl's loader reads no cache, so there is nothing
# for it to do.
make install INSTALL_ROOT=$PKG CONFLIBDIR=:

# BrlAPI's default authorisation is `keyfile:/etc/brlapi.key+polkit`, and
# neither half exists after `make install`: the key is generated only by an
# install with no INSTALL_ROOT, and the polkit action and rule are a separate
# target. The key would be one secret baked identically into every image, so
# the polkit half is the one installed: org.a11y.brlapi.write-display, with
# upstream's rule granting it to the brlapi group and testing no session, which
# is what makes it work here where nothing is ever active. postinstall.sh makes
# the group.
make install-polkit INSTALL_ROOT=$PKG

# /dev/vcsa IS ROOT-AND-tty-GROUP, so BRLTTY runs as a service rather than as
# the user. The ksvc script is the shape every other daemon here has, and it
# SKIPS rather than fails until /etc/brltty.conf says what to drive — a respawn
# loop around a daemon with nothing to do is a boot that never settles.
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
