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

if [ -f "$MARK/readline" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building readline..."

# Unpack the readline port's source tarball alone; its patches are not applied
READLINE_SRC=$(extract_port_source readline)

cd "$READLINE_SRC"
mkdir -p build && cd build

../configure \
    --host=$KDOS_TARGET \
    --prefix=/usr \
    --disable-static

make SHLIB_LIBS="-lncursesw"
make DESTDIR=$SYSROOT install

# kpkg prunes the HTML manuals from the readline port's package, so no
# package adopts these copies and they would outlive every install.
rm -f $SYSROOT/usr/share/doc/readline/{history,readline,rluserman}.html

rm -rf "$READLINE_SRC"
touch "$MARK/readline"
