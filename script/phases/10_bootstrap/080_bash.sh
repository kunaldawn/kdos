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

if [ -f "$MARK/bash" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building bash..."

# Extract bash and dependencies from ports
BASH_SRC=$(extract_port_source bash)

cd "$BASH_SRC"
mkdir -p build && cd build

../configure \
    --host=$KDOS_TARGET \
    --prefix=/usr \
    --with-curses \
    --enable-readline \
    --without-bash-malloc \
    --with-installed-readline \
    --disable-nls

make
make DESTDIR=$SYSROOT install

# kpkg prunes the HTML manuals from the bash port's package, so no package
# adopts these copies and they would outlive every install.
rm -f $SYSROOT/usr/share/doc/bash/{bash,bashref}.html

cd "$SYSROOT/usr/bin"
ln -sv bash sh

rm -rf "$BASH_SRC"
touch "$MARK/bash"
