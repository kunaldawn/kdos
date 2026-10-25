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

set -e
source script/phases/10_bootstrap/phase.env
source script/lib/port.sh

if [ -f "$MARK/gawk" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building gawk..."

GAWK_SRC=$(extract_port_source gawk)

cd "$GAWK_SRC"

./configure \
    --host=$KDOS_TARGET \
    --build=$(build-aux/config.guess) \
    --prefix=/usr \
    --sysconfdir=/etc \
    --without-readline \
    --without-mpfr \
    --disable-nls

make
make DESTDIR=$SYSROOT install

# The gawk port installs gawk.sh and its helpers in /etc/profile.d and
# leaves out gawk.csh and gawknotes.info; this copy matches it, so nothing
# here outlives the port's install over it.
rm -f $SYSROOT/etc/profile.d/gawk.csh $SYSROOT/usr/share/info/gawknotes.info

rm -rf "$GAWK_SRC"
touch "$MARK/gawk"
