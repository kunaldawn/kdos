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

if [ -f "$MARK/ncurses" ]; then
    exit 0
fi

echo ">>> Building ncurses..."

# Extract ncurses and dependencies from ports
NCURSES_SRC=$(extract_port_source ncurses)

cd "$NCURSES_SRC"
mkdir -p build1 && cd build1
../configure --prefix=$CROSS_SYSROOT
make -C include
make -C progs tic
install progs/tic $CROSS_SYSROOT/bin
cd ..

mkdir -p build && cd build

../configure \
    --host=$KDOS_TARGET \
    --build=$(../config.guess)    \
    --prefix=/usr \
    --mandir=/usr/share/man      \
    --with-manpage-format=normal \
    --with-shared                \
    --without-normal             \
    --with-cxx-shared            \
    --without-debug              \
    --without-ada                \
    --enable-widec               \
    --disable-stripping

make DESTDIR=$SYSROOT install

rm -rf "$NCURSES_SRC"
touch "$MARK/ncurses"
