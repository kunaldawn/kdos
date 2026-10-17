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

if [ -f "$MARK/findutils" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building findutils..."

FINDUTILS_SRC=$(extract_port_source findutils)

cd "$FINDUTILS_SRC"

./configure \
    --host=$KDOS_TARGET \
    --build=$(build-aux/config.guess) \
    --prefix=/usr \
    --localstatedir=/var/lib/locate \
    --disable-nls

make
make DESTDIR=$SYSROOT install

# locate and updatedb are plocate's. The findutils port leaves its own pair
# out of the package, so a copy installed here would be owned by nothing,
# never replaced, and found on $PATH ahead of /usr/sbin/updatedb.
rm -f $SYSROOT/usr/bin/locate $SYSROOT/usr/bin/updatedb $SYSROOT/usr/libexec/frcode \
    $SYSROOT/usr/share/man/man1/locate.1 $SYSROOT/usr/share/man/man1/updatedb.1 \
    $SYSROOT/usr/share/man/man5/locatedb.5
rmdir $SYSROOT/var/lib/locate 2>/dev/null || true

rm -rf "$FINDUTILS_SRC"
touch "$MARK/findutils"
