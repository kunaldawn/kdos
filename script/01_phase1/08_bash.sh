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
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/bash" ]; then
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
    --disable-static \
    --with-installed-readline

make
make DESTDIR=$SYSROOT install

cd "$SYSROOT/usr/bin"
ln -sv bash sh

rm -rf "$BASH_SRC"
touch "$MARK/bash"
