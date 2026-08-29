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

# A CHARACTER-CELL DESKTOP IS THE IDEAL THING TO READ, which is the one real
# accessibility advantage this project has and the reason this is a port rather
# than a note. BRLTTY reads the Linux console out of /dev/vcsa — the whole of
# tty1, the installer and every kdos-shell surface is a grid of cells it can
# take verbatim, with none of the guessing a screen reader does over a
# toolkit's accessibility tree.
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
# `,-all` ON EVERY LIST, and without it the rest are built as EXTERNAL loadable
# drivers rather than skipped. That is not free: the OLD eSpeak driver links
# `-lespeak`, which does not exist here (espeak-ng provides libespeak-ng), and
# the FileViewer screen driver links `-ltinfo`, which this ncurses does not
# build as a separate library. Naming a driver selects what is INTERNAL; only
# `-all` says what is not built at all.
#
# --with-braille-driver=-all because no braille display is attached to a build
# machine and every one of those drivers is a separate .so against a separate
# vendor library. The API server and the speech path are what this port is
# for; a display driver can be added the day there is a display.
#
# espeak-ng is what turns that into speech; without it BRLTTY drives a braille
# display and nothing else, which is a much smaller feature than the recipe
# claims. STATED LIMIT: boxed GUI applications remain unreachable — they have
# no cells and there is no at-spi registry on this host. docs/ACCESSIBILITY.md
# is the statement of record.
#
# --disable-x, --without-xorg: the hard rule, and BRLTTY's X support exists to
# read an X screen this system does not have. The API server stays on because
# it is how anything else on the machine asks BRLTTY to speak.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--disable-x \
	--without-xorg \
	--disable-java-bindings \
	--disable-ocaml-bindings \
	--disable-tcl-bindings \
	--disable-python-bindings \
	--with-screen-driver=lx,-all \
	--with-speech-driver=eSpeak-NG,-all \
	--with-braille-driver=-all
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

# /dev/vcsa IS ROOT-AND-tty-GROUP, so BRLTTY runs as a service rather than as
# the user. The ksvc script is the shape every other daemon here has, and it
# SKIPS rather than fails when no braille device is attached — a respawn loop
# around a daemon with no hardware is a boot that never settles.
install -Dm755 /dev/stdin $PKG/etc/init.d/65_brltty.sh <<'SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="brltty"
DAEMON="/usr/bin/brltty"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # No braille device and no explicit configuration means nothing to
        # drive. BRLTTY would exit, and a supervised daemon that exits is a
        # respawn loop; checking here is what keeps the boot quiet on the
        # overwhelming majority of machines that have no display attached.
        if [ ! -s /etc/brltty.conf ] && ! ls /dev/ttyUSB* /dev/ttyACM* >/dev/null 2>&1; then
            echo "[SKIP] $NAME: no braille device and no /etc/brltty.conf"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" -n
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
SH
